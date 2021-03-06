/*
 * =====================================================================================
 *
 *       Filename:  lcdc_panel_wvga_oled.c
 *
 *    Description:  Samsung WVGA panel(480x800) driver 
 *
 *        Version:  1.0
 *        Created:  03/25/2010 02:05:46 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Lu Ya
 *        Company:  ZTE Corp.
 *
 * =====================================================================================
 */
/* ========================================================================================
when         who        what, where, why                                  comment tag
--------     ----       -----------------------------                --------------------------
2011-01-31	 lkej		modify the code for detecting panel 			ZTE_LCD_LKEJ_20110131_001
2011-01-27	 lkej		merged boe and truly lcd driver				ZTE_LCD_LKEJ_20110127_001
==========================================================================================*/


#include "msm_fb.h"
#include <asm/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>

#define GPIO_LCD_BL_SC_OUT 57
#define GPIO_LCD_ID_DETECT  34

#define GPIO_SPI_CS          122
#define GPIO_SPI_SDO         123
#define GPIO_SPI_SCLK        124
#define GPIO_SPI_SDI         132

#define LCD_BL_LEVEL 12
#define lcd_bl_max   lcd_bl_level-1
#define lcd_bl_min   0

typedef enum
{
	LCD_PANEL_NONE = 0,
	LCD_PANEL_LEAD_HVGA,
	LCD_PANEL_TRULY_HVGA,
	LCD_PANEL_BOE_HVGA,
}LCD_PANEL_TYPE;

static LCD_PANEL_TYPE g_lcd_panel_type = LCD_PANEL_NONE;

static boolean is_firsttime = true;		
static boolean isOdd = true;
extern u32 LcdPanleID;   
static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;
static int panel_reset;
static bool onewiremode = true;

static struct msm_panel_common_pdata * lcdc_tft_pdata;
static void gpio_lcd_9bit_emuspi_write_one_index(unsigned short addr);
static void gpio_lcd_9bit_emuspi_write_one_data(unsigned short data);
static void gpio_lcd_eR61581_emuspi_read_id_para(unsigned short addr, unsigned int *data1);
static void gpio_lcd_ILI9481_emuspi_read_id_para(unsigned short addr, unsigned int *data1);	
static void lcdc_lead_init(void);
static void lcdc_truly_init(void);
static void lcd_panel_init(void);
static void lcdc_set_bl(struct msm_fb_data_type *mfd);
void lcdc_lead_sleep(void);
void lcdc_truly_sleep(void);
static void spi_init(void);
static int lcdc_panel_on(struct platform_device *pdev);
static int lcdc_panel_off(struct platform_device *pdev);
static LCD_PANEL_TYPE lcd_panel_detect(void);

static void select_1wire_mode(void)
{
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(120);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
	udelay(300);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(600);
	
}
static void send_bkl_address(void)
{
	unsigned int i,j;
	i = 0x72;
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(10);
	printk("[LY] send_bkl_address \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(10);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(180);
		}
		else
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(180);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
	udelay(10);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
}

static void send_bkl_data(int level)
{
	unsigned int i,j;
	i = level & 0x1F;
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(10);
	printk("[LY] send_bkl_data \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(10);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(180);
		}
		else
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(180);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
	udelay(10);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
}

static void lcdc_set_bl(struct msm_fb_data_type *mfd)
{

       /*value range is 1--32*/
    int current_lel = mfd->bl_level;
    unsigned long flags;
	
    printk("[ZYF] lcdc_set_bl level=%d, %d\n", 
		   current_lel , mfd->panel_power_on);

    if(!mfd->panel_power_on)
	{
    	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);			/
	    return;
    }

    if(current_lel < 1)
    {
        current_lel = 0;
    }
    if(current_lel > 32)
    {
        current_lel = 32;
    }

    
    local_irq_save(flags);
    if(current_lel==0)
    {
    	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
		mdelay(3);
		onewiremode = FALSE;		
    }
    else 
	{
		if(!onewiremode)	//select 1 wire mode
		{
			printk("[LY] before select_1wire_mode\n");
			select_1wire_mode();
			onewiremode = TRUE;
		}
		send_bkl_address();
		send_bkl_data(current_lel-1);
	}
    local_irq_restore(flags);	
}
static int lcdc_panel_on(struct platform_device *pdev)
{
	spi_init();
/
	if(!is_firsttime)
	{
		lcd_panel_init();
		
	}
	else
	{
		is_firsttime = false;
	}
/
	return 0;
}
static void gpio_lcd_9bit_emuspi_write_one_index(unsigned short addr)
	{
		unsigned short SBit,SBuffer;
		unsigned char BitCounter;
		
		SBuffer=addr;
		gpio_direction_output(spi_cs, 0);
		for(BitCounter=0;BitCounter<9;BitCounter++)
		{
			SBit = SBuffer&0x100;
			if(SBit)
				gpio_direction_output(spi_sdo, 1);
			else
				gpio_direction_output(spi_sdo, 0);
				
			gpio_direction_output(spi_sclk, 0);
			gpio_direction_output(spi_sclk, 1);
			SBuffer = SBuffer<<1;
		}
		gpio_direction_output(spi_cs, 1);
	}

static void gpio_lcd_9bit_emuspi_write_one_data(unsigned short data)
	{
		unsigned short SBit,SBuffer;
		unsigned char BitCounter;
		
		SBuffer= data | 0x100;
		gpio_direction_output(spi_cs, 0);
		
		for(BitCounter=0;BitCounter<9;BitCounter++)
		{
			SBit = SBuffer&0x100;
			if(SBit)
				gpio_direction_output(spi_sdo, 1);
			else
				gpio_direction_output(spi_sdo, 0);
				
			gpio_direction_output(spi_sclk, 0);
			gpio_direction_output(spi_sclk, 1);
			SBuffer = SBuffer<<1;
		}
		gpio_direction_output(spi_cs, 1);
	}
	
static void gpio_lcd_eR61581_emuspi_read_id_para(unsigned short addr, unsigned int *data1)					
{
			
			int j,i;
			unsigned int dbit,bits1;		
			i = addr;
			
			gpio_direction_output(GPIO_SPI_CS, 0);
			for (j = 0; j < 9; j++) {
		
				if (i & 0x100)
					gpio_direction_output(GPIO_SPI_SDO, 1); 
				else
					gpio_direction_output(GPIO_SPI_SDO, 0); 
		
				gpio_direction_output(GPIO_SPI_SCLK, 0); 
				gpio_direction_output(GPIO_SPI_SCLK, 1);
				i <<= 1;
			}
			bits1=0;	
			for (j = 0; j < 40; j++) {//read 4 byte
	
				gpio_direction_output(GPIO_SPI_SCLK, 0); 
				dbit=gpio_get_value(GPIO_SPI_SDI);
				gpio_direction_output(GPIO_SPI_SCLK, 1);
				bits1 = 2*bits1+dbit;
			}
			*data1 =  bits1;
			gpio_direction_output(GPIO_SPI_CS, 1);
			printk("read_lcd er61581 PID:0x%x\n!",bits1);
		
}

static void gpio_lcd_ILI9481_emuspi_read_id_para(unsigned short addr, unsigned int *data1)					
{
			
			int j,i;
			unsigned int dbit,bits1;		
			i = addr;
			
			gpio_direction_output(GPIO_SPI_CS, 0);
			for (j = 0; j < 9; j++) {
		
				if (i & 0x100)
					gpio_direction_output(GPIO_SPI_SDO, 1); 
				else
					gpio_direction_output(GPIO_SPI_SDO, 0); 
		
				gpio_direction_output(GPIO_SPI_SCLK, 0); 
				gpio_direction_output(GPIO_SPI_SCLK, 1);
				i <<= 1;
			}
			bits1=0;	
			for (j = 0; j < 33; j++) {//read 4 byte
	
				gpio_direction_output(GPIO_SPI_SCLK, 0); 
				dbit=gpio_get_value(GPIO_SPI_SDI);
				gpio_direction_output(GPIO_SPI_SCLK, 1);
				bits1 = 2*bits1+dbit;
			}
			*data1 =  bits1;
			gpio_direction_output(GPIO_SPI_CS, 1);
			printk("read_lcd ili9481 PID:0x%x\n!",bits1);
		
}
static void lcdc_lead_init(void)
{
	gpio_lcd_9bit_emuspi_write_one_index(0x11);
	msleep(20);
	gpio_lcd_9bit_emuspi_write_one_index(0xC6);
	gpio_lcd_9bit_emuspi_write_one_data(0x9B);

	gpio_lcd_9bit_emuspi_write_one_index(0xD0);
	gpio_lcd_9bit_emuspi_write_one_data(0x07);
	gpio_lcd_9bit_emuspi_write_one_data(0x47);
	gpio_lcd_9bit_emuspi_write_one_data(0x1D);
	
	gpio_lcd_9bit_emuspi_write_one_index(0xD1);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x35);//10
	gpio_lcd_9bit_emuspi_write_one_data(0x14);
	
	gpio_lcd_9bit_emuspi_write_one_index(0xD2);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_data(0x11);
	
	gpio_lcd_9bit_emuspi_write_one_index(0xC0);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);//04
	gpio_lcd_9bit_emuspi_write_one_data(0x3B);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x02);
	gpio_lcd_9bit_emuspi_write_one_data(0x11);
	
	gpio_lcd_9bit_emuspi_write_one_index(0xC5);
	gpio_lcd_9bit_emuspi_write_one_data(0x03);
	
	gpio_lcd_9bit_emuspi_write_one_index(0xC8);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x53);
	gpio_lcd_9bit_emuspi_write_one_data(0x17);
	gpio_lcd_9bit_emuspi_write_one_data(0x45);
	gpio_lcd_9bit_emuspi_write_one_data(0x0A);
	gpio_lcd_9bit_emuspi_write_one_data(0x1A);
	gpio_lcd_9bit_emuspi_write_one_data(0x06);
	gpio_lcd_9bit_emuspi_write_one_data(0x42);
	gpio_lcd_9bit_emuspi_write_one_data(0x77);
	gpio_lcd_9bit_emuspi_write_one_data(0x54);
	gpio_lcd_9bit_emuspi_write_one_data(0x12);
	gpio_lcd_9bit_emuspi_write_one_data(0x0C);
	
	gpio_lcd_9bit_emuspi_write_one_index(0x36);
	gpio_lcd_9bit_emuspi_write_one_data(0x48); // MX,MY0x88
	gpio_lcd_9bit_emuspi_write_one_index(0x3A);
	gpio_lcd_9bit_emuspi_write_one_data(0x66); //18bit
	gpio_lcd_9bit_emuspi_write_one_index(0x2A);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_data(0x3F);
	gpio_lcd_9bit_emuspi_write_one_index(0x2B);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_data(0xDF);
	msleep(120);
	gpio_lcd_9bit_emuspi_write_one_index(0xB4);
	gpio_lcd_9bit_emuspi_write_one_data(0x10);  //DPI Interface (RGB),Internal system clock

	gpio_lcd_9bit_emuspi_write_one_index(0x29);   //display_on
	gpio_lcd_9bit_emuspi_write_one_index(0x21);   //Enter_invert_mode//0x20
	gpio_lcd_9bit_emuspi_write_one_index(0x2C);   //Write_memory_start
    msleep(50); 

	printk("lcd module TFT LEAD init finish\n!");
}

static void lcdc_boe_init(void)
{	
	gpio_lcd_9bit_emuspi_write_one_index(0xB0);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_index(0xB3);
	gpio_lcd_9bit_emuspi_write_one_data(0x02);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x10);

	gpio_lcd_9bit_emuspi_write_one_index(0xB4);
	gpio_lcd_9bit_emuspi_write_one_data(0x11);
	gpio_lcd_9bit_emuspi_write_one_index(0xC0);	//Panel Driving Setting
	gpio_lcd_9bit_emuspi_write_one_data(0x16); //13
	gpio_lcd_9bit_emuspi_write_one_data(0x3B);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x43);
	gpio_lcd_9bit_emuspi_write_one_index(0xC1);
	gpio_lcd_9bit_emuspi_write_one_data(0x08);
	gpio_lcd_9bit_emuspi_write_one_data(0x15);
	gpio_lcd_9bit_emuspi_write_one_data(0x08);
	gpio_lcd_9bit_emuspi_write_one_data(0x08);
	gpio_lcd_9bit_emuspi_write_one_index(0xC4);
	gpio_lcd_9bit_emuspi_write_one_data(0x15);
	gpio_lcd_9bit_emuspi_write_one_data(0x03);
	gpio_lcd_9bit_emuspi_write_one_data(0x03);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_index(0xC6);
	gpio_lcd_9bit_emuspi_write_one_data(0x33);//0x02

	gpio_lcd_9bit_emuspi_write_one_index(0xC8);
	gpio_lcd_9bit_emuspi_write_one_data(0x0C);
	gpio_lcd_9bit_emuspi_write_one_data(0x05);
	gpio_lcd_9bit_emuspi_write_one_data(0x0A);
	gpio_lcd_9bit_emuspi_write_one_data(0x6B);
	gpio_lcd_9bit_emuspi_write_one_data(0x04);
	gpio_lcd_9bit_emuspi_write_one_data(0x06);
	gpio_lcd_9bit_emuspi_write_one_data(0x15);
	gpio_lcd_9bit_emuspi_write_one_data(0x10);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x31);
	gpio_lcd_9bit_emuspi_write_one_data(0x10);
	gpio_lcd_9bit_emuspi_write_one_data(0x15);
	gpio_lcd_9bit_emuspi_write_one_data(0x06);
	gpio_lcd_9bit_emuspi_write_one_data(0x64);
	gpio_lcd_9bit_emuspi_write_one_data(0x0B);
	gpio_lcd_9bit_emuspi_write_one_data(0x0A);
	gpio_lcd_9bit_emuspi_write_one_data(0x05);
	gpio_lcd_9bit_emuspi_write_one_data(0x0C);
	gpio_lcd_9bit_emuspi_write_one_data(0x31);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	msleep(100);

	gpio_lcd_9bit_emuspi_write_one_index(0x2A);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_data(0x3F);
	gpio_lcd_9bit_emuspi_write_one_index(0x2B);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x01);
	gpio_lcd_9bit_emuspi_write_one_data(0xDF);

	gpio_lcd_9bit_emuspi_write_one_index(0x35);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);//00
	gpio_lcd_9bit_emuspi_write_one_index(0x36);
	gpio_lcd_9bit_emuspi_write_one_data(0xC0); //set_address_mode
	gpio_lcd_9bit_emuspi_write_one_index(0x3A);
	gpio_lcd_9bit_emuspi_write_one_data(0x66); //18bit
	gpio_lcd_9bit_emuspi_write_one_index(0x44);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x01); //01

	gpio_lcd_9bit_emuspi_write_one_index(0xD0);
	gpio_lcd_9bit_emuspi_write_one_data(0x07);
	gpio_lcd_9bit_emuspi_write_one_data(0x07);
	gpio_lcd_9bit_emuspi_write_one_data(0x14);
	gpio_lcd_9bit_emuspi_write_one_data(0xA2);
	gpio_lcd_9bit_emuspi_write_one_index(0xD1);
	gpio_lcd_9bit_emuspi_write_one_data(0x03);
	gpio_lcd_9bit_emuspi_write_one_data(0x33);
	gpio_lcd_9bit_emuspi_write_one_data(0x0A);
	gpio_lcd_9bit_emuspi_write_one_index(0xD2);
	gpio_lcd_9bit_emuspi_write_one_data(0x03);
	gpio_lcd_9bit_emuspi_write_one_data(0x04);
	gpio_lcd_9bit_emuspi_write_one_data(0x04);

	gpio_lcd_9bit_emuspi_write_one_index(0x11);
	msleep(100);

	gpio_lcd_9bit_emuspi_write_one_index(0x29);
	gpio_lcd_9bit_emuspi_write_one_index(0x2C);

	printk("[hp@lcd&fb]:lcd module BOE init exit\n!");
}

void lcdc_truly_init(void)
{	
		gpio_lcd_9bit_emuspi_write_one_index(0x11);
		msleep(20);
		gpio_lcd_9bit_emuspi_write_one_index(0xC6);
		gpio_lcd_9bit_emuspi_write_one_data(0x9B);
	
		gpio_lcd_9bit_emuspi_write_one_index(0xD0);
		gpio_lcd_9bit_emuspi_write_one_data(0x07);
		gpio_lcd_9bit_emuspi_write_one_data(0x47);
		gpio_lcd_9bit_emuspi_write_one_data(0x1D);
		
		gpio_lcd_9bit_emuspi_write_one_index(0xD1);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x35);//10
		gpio_lcd_9bit_emuspi_write_one_data(0x14);
		
		gpio_lcd_9bit_emuspi_write_one_index(0xD2);
		gpio_lcd_9bit_emuspi_write_one_data(0x01);
		gpio_lcd_9bit_emuspi_write_one_data(0x11);
		
		gpio_lcd_9bit_emuspi_write_one_index(0xC0);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);//04
		gpio_lcd_9bit_emuspi_write_one_data(0x3B);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x02);
		gpio_lcd_9bit_emuspi_write_one_data(0x11);
		
		gpio_lcd_9bit_emuspi_write_one_index(0xC5);
		gpio_lcd_9bit_emuspi_write_one_data(0x03);
		
		gpio_lcd_9bit_emuspi_write_one_index(0xC8);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x53);
		gpio_lcd_9bit_emuspi_write_one_data(0x17);
		gpio_lcd_9bit_emuspi_write_one_data(0x45);
		gpio_lcd_9bit_emuspi_write_one_data(0x0A);
		gpio_lcd_9bit_emuspi_write_one_data(0x1A);
		gpio_lcd_9bit_emuspi_write_one_data(0x06);
		gpio_lcd_9bit_emuspi_write_one_data(0x42);
		gpio_lcd_9bit_emuspi_write_one_data(0x77);
		gpio_lcd_9bit_emuspi_write_one_data(0x54);
		gpio_lcd_9bit_emuspi_write_one_data(0x12);
		gpio_lcd_9bit_emuspi_write_one_data(0x0C);
		
		gpio_lcd_9bit_emuspi_write_one_index(0x36);
		gpio_lcd_9bit_emuspi_write_one_data(0x48); // MX,MY0x88
		gpio_lcd_9bit_emuspi_write_one_index(0x3A);
		gpio_lcd_9bit_emuspi_write_one_data(0x66); //18bit
		gpio_lcd_9bit_emuspi_write_one_index(0x2A);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x01);
		gpio_lcd_9bit_emuspi_write_one_data(0x3F);
		gpio_lcd_9bit_emuspi_write_one_index(0x2B);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x00);
		gpio_lcd_9bit_emuspi_write_one_data(0x01);
		gpio_lcd_9bit_emuspi_write_one_data(0xDF);
		msleep(120);
		gpio_lcd_9bit_emuspi_write_one_index(0xB4);
		gpio_lcd_9bit_emuspi_write_one_data(0x10);	//DPI Interface (RGB),Internal system clock
	
		gpio_lcd_9bit_emuspi_write_one_index(0x29);   //display_on
		gpio_lcd_9bit_emuspi_write_one_index(0x21);   //Enter_invert_mode//0x20
		gpio_lcd_9bit_emuspi_write_one_index(0x2C);   //Write_memory_start
		msleep(50); 
		printk("lcd module TFT TRULY init finish\n!");

}


static void spi_init(void)
{
	spi_sclk = *(lcdc_tft_pdata->gpio_num);
	spi_cs   = *(lcdc_tft_pdata->gpio_num + 1);
	spi_sdi  = *(lcdc_tft_pdata->gpio_num + 2);
	spi_sdo  = *(lcdc_tft_pdata->gpio_num + 3);
	panel_reset = *(lcdc_tft_pdata->gpio_num + 4);
	printk("spi_init\n!");
	printk("spi_init spi_sclk = %d,spi_cs = %d,spi_sdi = %d,spi_sdo = %d\n",spi_sclk,spi_cs,spi_sdi,spi_sdo);

	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdo, 1);
	gpio_set_value(spi_cs, 1);
	gpio_direction_input(spi_sdi);
}
static void lcdc_boe_sleep(void)
{
	gpio_lcd_9bit_emuspi_write_one_index(0x10);
	msleep(10);	
}
void lcdc_truly_sleep(void)
{
	gpio_lcd_9bit_emuspi_write_one_index(0x28);
	msleep(300);
	/*******power off  ******************/
	gpio_lcd_9bit_emuspi_write_one_index(0xD0);
	gpio_lcd_9bit_emuspi_write_one_data(0x07);
	gpio_lcd_9bit_emuspi_write_one_data(0x42);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);

	gpio_lcd_9bit_emuspi_write_one_data(0xD2);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x77); 
	msleep(300);
	gpio_lcd_9bit_emuspi_write_one_index(0x10);
	msleep(200);	
}
void lcdc_lead_sleep(void)
{
	gpio_lcd_9bit_emuspi_write_one_index(0x28);
	msleep(300);
	/*******power off  ******************/
	gpio_lcd_9bit_emuspi_write_one_index(0xD0);
	gpio_lcd_9bit_emuspi_write_one_data(0x07);
	gpio_lcd_9bit_emuspi_write_one_data(0x42);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);

	gpio_lcd_9bit_emuspi_write_one_data(0xD2);
	gpio_lcd_9bit_emuspi_write_one_data(0x00);
	gpio_lcd_9bit_emuspi_write_one_data(0x77); 
	msleep(300);
	gpio_lcd_9bit_emuspi_write_one_index(0x10);
	msleep(200);	
}
static int lcdc_panel_off(struct platform_device *pdev)
{
	printk("lcdc_panel_off , g_lcd_panel_type is %d(1 LEAD. 2 TRULY. 3 OLED. )\n",g_lcd_panel_type);

	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_TRULY_HVGA:
			lcdc_truly_sleep();
			break;
		case LCD_PANEL_LEAD_HVGA:
			lcdc_lead_sleep();
			break;
		case LCD_PANEL_BOE_HVGA:
			lcdc_boe_sleep();
			break;	
		default:
			break;
	}
	msleep(300);
	gpio_direction_output(panel_reset, 0);
	
	gpio_direction_output(spi_sclk, 0);
	gpio_direction_output(spi_sdi, 0);
	gpio_direction_output(spi_sdo, 0);
	gpio_direction_output(spi_cs, 0);
	msleep(300);
	return 0;
}

static LCD_PANEL_TYPE lcd_panel_detect(void)
{
	unsigned int id;
	spi_init();
	gpio_lcd_eR61581_emuspi_read_id_para(0xBF,&id);
	gpio_lcd_9bit_emuspi_write_one_index(0x2C);
	if(id == 0x01221581)
	{
		printk("boe id is 0x%x\n",id);
		return LCD_PANEL_BOE_HVGA;
	}
	else
	{
		gpio_direction_input(GPIO_LCD_ID_DETECT);
		if(gpio_get_value(GPIO_LCD_ID_DETECT))
		{
			printk("truly id is 0x%x\n",id);
			return LCD_PANEL_TRULY_HVGA;
		}
		else
		{
			printk("lead id is 0x%x\n",id);
			return LCD_PANEL_LEAD_HVGA;
		}
	}	
}

void lcd_panel_init(void)
{
	unsigned int id;
	msleep(10);
	gpio_direction_output(panel_reset, 1);
	msleep(10);						//
	gpio_direction_output(panel_reset, 0);
	msleep(20);						//
	gpio_direction_output(panel_reset, 1);
	msleep(20);	
	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_TRULY_HVGA:
			lcdc_truly_init();
			break;
		case LCD_PANEL_LEAD_HVGA:
			lcdc_lead_init();
			break;
		case LCD_PANEL_BOE_HVGA:
			lcdc_boe_init();
			break;
		default:
			break;
	}	
	msleep(300);
}

static struct msm_fb_panel_data lcdc_tft_panel_data = {
       .panel_info = {.bl_max = 32},
	.on = lcdc_panel_on,
	.off = lcdc_panel_off,
       .set_backlight = lcdc_set_bl,
};

static struct platform_device this_device = {
	.name   = "lcdc_panel_qvga",
	.id	= 1,
	.dev	= {
		.platform_data = &lcdc_tft_panel_data,
	}
};
static int __init lcdc_panel_probe(struct platform_device *pdev)
{
	struct msm_panel_info *pinfo;
	int ret;

	if(pdev->id == 0) {     
		lcdc_tft_pdata = pdev->dev.platform_data;
		lcdc_tft_pdata->panel_config_gpio(1);  
		g_lcd_panel_type = lcd_panel_detect();

		if(g_lcd_panel_type==LCD_PANEL_TRULY_HVGA)
		{
			pinfo = &lcdc_tft_panel_data.panel_info;
			pinfo->lcdc.h_back_porch = 20;
			pinfo->lcdc.h_front_porch = 40;
			pinfo->lcdc.h_pulse_width = 10;
			pinfo->lcdc.v_back_porch = 2;
			pinfo->lcdc.v_front_porch = 4;
			pinfo->lcdc.v_pulse_width = 2;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
			printk("@lkej!lcd_panel_truly_probe\n");

		}
		else if(g_lcd_panel_type==LCD_PANEL_LEAD_HVGA)
		{
			pinfo = &lcdc_tft_panel_data.panel_info;
			pinfo->lcdc.h_back_porch = 20;
			pinfo->lcdc.h_front_porch = 40;
			pinfo->lcdc.h_pulse_width = 10;
			pinfo->lcdc.v_back_porch = 2;
			pinfo->lcdc.v_front_porch = 4;
			pinfo->lcdc.v_pulse_width = 2;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
			printk("@lkej!lcd_panel_lead_probe\n");
		}
		else
		{
			pinfo = &lcdc_tft_panel_data.panel_info;
			pinfo->lcdc.h_back_porch = 20;
			pinfo->lcdc.h_front_porch = 10;
			pinfo->lcdc.h_pulse_width = 10;
			pinfo->lcdc.v_back_porch = 2;
			pinfo->lcdc.v_front_porch = 4;
			pinfo->lcdc.v_pulse_width = 2;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
			printk("@lkej!lcd_panel_boe_probe\n");
		}	
		pinfo->xres = 320;
		pinfo->yres = 480;		
		pinfo->type = LCDC_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 18;
		pinfo->fb_num = 2;
		switch(g_lcd_panel_type)
		{
			case LCD_PANEL_TRULY_HVGA:
				LcdPanleID=(u32)41;   
				pinfo->clk_rate = 8192000;
				ret = platform_device_register(&this_device);
				break;
			case LCD_PANEL_BOE_HVGA:
				LcdPanleID=(u32)40;   
				pinfo->clk_rate = 8192000;//6144000;
				ret = platform_device_register(&this_device);
				break;
			case LCD_PANEL_LEAD_HVGA:
				pinfo->clk_rate = 8192000;
				LcdPanleID=(u32)42;   
				ret = platform_device_register(&this_device);
				printk("lcd_panel_probe1 **lkej**\n");
				break;
			default:
				break;
		}		
		return 0;
	 	
	}
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_panel_probe,
	.driver = {
		.name   = "lcdc_panel_qvga",
	},
};
static int __init lcdc_tft_panel_init(void)
{
	int ret;

	ret = platform_driver_register(&this_driver);

	return ret;
}

module_init(lcdc_tft_panel_init);

