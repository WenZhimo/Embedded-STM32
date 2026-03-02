//#include "./stm32f1xx_it.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/BEEP/beep.h"
//#include "./BSP/EXTI/exti.h"
#include "./BSP/KEY/key.h"
//#include "./BSP/WDG/wdg.h"
//#include "./BSP/TIMER/btim.h"
//#include "./BSP/TIMER/gtim.h"
//#include "./BSP/OLED/OLED.h"
//#include "./BSP/DHT11/DHT11.h"
#include "string.h"
#include "./BSP/RTC/rtc.h"
//#include "./BSP/DMA/dma.h"
#include "./BSP/LCD/lcd.h"
#include "BSP/Text_Animation/Text_Animation.h"

char* weekdays[]={"星期天","星期一","星期二","星期三",
                  "星期四","星期五","星期六"};

int main(void)
{
    HAL_Init();
    sys_stm32_clock_init(RCC_PLL_MUL9);
    delay_init(72);
    usart_init(115200);
    led_init();
    beep_init();
    key_init();
    //OLED_Init();
    rtc_init();                             /* 初始化RTC */
    lcd_init();                                         /* 初始化LCD */
    
    /* RTC相关变量 */
    uint8_t tbuf[40];
    
    uint8_t x = 95;
    uint8_t lcd_id[12];
    sprintf((char *)lcd_id, "LCD ID:%04X", lcddev.id);  /* 将LCD ID打印到lcd_id数组 */
    g_back_color = BLACK ;
    g_point_color=WHITE;
    lcd_clear(BLACK);
    my_lcd_show_image(0,27,320,426,gImage_MutsumiSaki320_426);
    //my_lcd_show_string(10,20,lcd_id,32,0,g_point_color);
    
    uint8_t len;
    uint16_t times = 0;
    UART_HandleTypeDef g_uart_handle=g_uart1_handle;
    int ox=0,oy = 0,bx=1,by=1;
    
    while (1)
    {

//        lcd_clear(BLACK);
//        lcd_draw_circle(ox+=bx,oy+=by,3,g_point_color);
//        if(ox>=320)bx=-1;
//        if(oy>=480)by=-1;
//        if(ox<=0)bx=1;
//        if(oy<=0)by=1;

        delay_ms(5);
    }
}






