#include "myinclude.h"
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "dma2d.h"
#include "fatfs.h"
#include "sdio.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fmc.h"

/* 系统初始化任务：负责按顺序初始化各个硬件外设和软件模块
   * 功能说明：
   *   1. 初始化SDRAM外部存储器
   *   2. 初始化LCD显示屏和DMA2D硬件加速模块
   *   3. 初始化USART1串口通信
   *   4. 初始化SD卡文件系统
   *   5. 初始化EUBF字体库系统
   *   6. 启动ADC1模数转换
   *   7. 启动定时器3
   * 
   * 事件组机制：
   *   - 使用xSystemInitEventGroupHandle事件组跟踪各模块初始化状态
   *   - 每完成一个模块初始化，调用xEventGroupSetBits()设置对应标志位
   *   - 当所有模块初始化完成后，调用vTaskDelete(NULL)删除当前任务
   * 
   * 参数：
   *   argument - FreeRTOS任务参数（本任务未使用）
   */
void AppTask_SYS_INIT(void *argument)
{
  /* USER CODE BEGIN AppTask_SYS_INIT */
  //==============================  初始化 SDRAM  =============================
    SDRAM_InitSequence();
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_SDRAM_READY);
    // ============================= 初始化 LCD 屏幕 ============================= 
    lcd_init();
    lcd_clear(BLACK);
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_LCD_READY);
    lcd_dma2d_init();     // 初始化DMA2D硬件加速模块，用于LCD图形绘制加速
    lcd_dma2d_clear(BLACK);
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_DMA2D_READY);
    // ============================= 初始化USART1 =============================
    MyUSART_Init();
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_USART1_READY);
    // ============================= 初始化EUBF ============================= 
    EUBF_Port_Config_t font_sd_config = {
        .Open     = SD_FastSlot_Open_UTF8,  // 文件打开接口，支持UTF-8编码
        .Close    = (void (*)(int8_t))SD_FastSlot_Close, // 文件关闭接口，类型转换避免编译警告
        .ReadAt   = SD_FastSlot_Read,       // 文件随机读取接口
        .RootPath = "0:/"                   // SD卡根目录路径
    };
    EUBF_Init(&font_sd_config);
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_EUBF_READY);
    //============================== 启动ADC1转换 =============================
    HAL_ADC_Start_IT(&hadc1);
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_ADC_READY);
    //============================== 启动定时器3 =============================
    HAL_TIM_Base_Start(&htim3);
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_TIM3_READY);
  /* Infinite loop */
  for(;;)
  {
    
    // ============================= 初始化SD卡 ============================= 
    SD_Mount_StateMachine();
    if(SD_Is_Mounted() == 1){
      // SD卡挂载成功，在LCD上显示实验标题
      lcd_dma2d_show_eubf_str(0, 435, (char*)"事件组实验", "BoutiqueBitmap7x7_Scan_Line", 12, Auqamarin);
      lcd_dma2d_show_eubf_str(270, 435, (char*)"文于止墨", "BoutiqueBitmap7x7_Scan_Line", 12, Auqamarin);
      lcd_dma2d_show_eubf_str(0, 450, (char*)"Event Group\nExperiment", "3x3等宽monospace", 12, Auqamarin);
      lcd_dma2d_show_eubf_str(210, 450, (char*)" WenZhimo\n@20260412", "3x3等宽monospace", 12, Auqamarin);
      lcd_dma2d_update_screen();
      xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_SD_MOUNTED);
    }
    //============================== 检查所有模块初始化状态 =============================
    if(xEventGroupGetBits(xSystemInitEventGroupHandle) == 
                                  (SYSTEM_INIT_EVENT_SDRAM_READY | 
                                  SYSTEM_INIT_EVENT_LCD_READY | 
                                  SYSTEM_INIT_EVENT_DMA2D_READY | 
                                  SYSTEM_INIT_EVENT_USART1_READY | 
                                  SYSTEM_INIT_EVENT_TIM3_READY | 
                                  SYSTEM_INIT_EVENT_SD_MOUNTED | 
                                  SYSTEM_INIT_EVENT_EUBF_READY  | 
                                  SYSTEM_INIT_EVENT_ADC_READY)){
      // 所有模块初始化完成，删除当前任务以释放系统资源
      vTaskDelete(NULL);
    }
    osDelay(1);
  }
  /* USER CODE END AppTask_SYS_INIT */
}