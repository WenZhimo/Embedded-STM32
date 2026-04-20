#include "myinclude.h"
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "dma2d.h"
#include "fatfs.h"
#include "rng.h"
#include "sdio.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fmc.h"
#include "sodium.h"
#include <stdio.h>
#include <string.h>



// 实现底层的随机数填充函数
static void stm32_randombytes_buf(void * const buf, const size_t size);
// 构造 libsodium 需要的实现结构体
static randombytes_implementation stm32_rng_impl;
// libsodium全局初始化函数
int Crypto_Init(void);



/* 系统初始化任务：负责按顺序初始化各个硬件外设和软件模块
   * 功能说明：
   *   1. 初始化SDRAM外部存储器
   *   2. 初始化LCD显示屏和DMA2D硬件加速模块
   *   3. 初始化USART1串口通信
   *   4. 初始化SD卡文件系统
   *   5. 初始化EUBF字体库系统
   *   6. 启动ADC1模数转换
   *   7. 启动定时器3
   *   8. 初始化libsodium加密库
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
  // ============================= 初始化USART1 =============================
    MyUSART_Init();
    printf("USART1 initialized\n");
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_USART1_READY);
  //==============================  初始化 SDRAM  =============================
    SDRAM_InitSequence();
    printf("SDRAM initialized\n");
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_SDRAM_READY);
    // ============================= 初始化 LCD 屏幕 ============================= 
    lcd_init();
    lcd_clear(BLACK);
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_LCD_READY);
    lcd_dma2d_init();     // 初始化DMA2D硬件加速模块，用于LCD图形绘制加速
    lcd_dma2d_clear(BLACK);
    printf("LCD/DMA2D initialized\n");
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_DMA2D_READY);
    
    // ============================= 初始化EUBF ============================= 
    EUBF_Port_Config_t font_sd_config = {
        .Open     = SD_FastSlot_Open_UTF8,  // 文件打开接口，支持UTF-8编码
        .Close    = (void (*)(int8_t))SD_FastSlot_Close, // 文件关闭接口，类型转换避免编译警告
        .ReadAt   = SD_FastSlot_Read,       // 文件随机读取接口
        .RootPath = "0:/"                   // SD卡根目录路径
    };
    EUBF_Init(&font_sd_config);
    printf("EUBF initialized\n");
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_EUBF_READY);
    //============================== 启动ADC1转换 =============================
    HAL_ADC_Start_IT(&hadc1);
    printf("ADC1 initialized\n");
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_ADC_READY);
    //============================== 启动定时器3 =============================
    HAL_TIM_Base_Start(&htim3);
    printf("TIM3 initialized\n");
    xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_TIM3_READY);
  /* Infinite loop */
  for(;;)
  {
    //============================== 初始化libsodium库 =============================
    Crypto_Init();
    if(Crypto_Init() != -1){ // 初始化libsodium库，准备使用加密功能
      printf("libsodium initialized\n");
      xEventGroupSetBits(xSystemInitEventGroupHandle, SYSTEM_INIT_EVENT_SODIUM_READY);
    }
    
    // ============================= 初始化SD卡 ============================= 
    SD_Mount_StateMachine();
    if(SD_Is_Mounted() == 1){
      // SD卡挂载成功，在LCD上显示实验标题
      lcd_dma2d_show_eubf_str(0, 435, (char*)"事件组实验", "BoutiqueBitmap7x7_Scan_Line", 12, Auqamarin);
      lcd_dma2d_show_eubf_str(270, 435, (char*)"文于止墨", "BoutiqueBitmap7x7_Scan_Line", 12, Auqamarin);
      lcd_dma2d_show_eubf_str(0, 450, (char*)"Event Group\nExperiment", "3x3等宽monospace", 12, Auqamarin);
      lcd_dma2d_show_eubf_str(210, 450, (char*)" WenZhimo\n@20260412", "3x3等宽monospace", 12, Auqamarin);
      lcd_dma2d_update_screen();
      printf("SD card mounted successfully\n");
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
                                  SYSTEM_INIT_EVENT_ADC_READY | 
                                  SYSTEM_INIT_EVENT_SODIUM_READY)){
      // 所有模块初始化完成，删除当前任务以释放系统资源
      printf("All modules initialized\n");
      LEDG(0);
      vTaskDelete(NULL);
    }
    osDelay(1);
  }
  /* USER CODE END AppTask_SYS_INIT */
}


// 实现底层的随机数填充函数
static void stm32_randombytes_buf(void * const buf, const size_t size) {
    unsigned char *p = (unsigned char *)buf;
    size_t i;
    for (i = 0; i < size; i++) {
        uint32_t random_val;
        // 每次生成 32 位随机数，这里为了简单演示，每次取 1 字节
        // 实际工程中可以优化为每获取一次 uint32_t，提取 4 个字节以提高效率
        HAL_RNG_GenerateRandomNumber(&hrng, &random_val);
        p[i] = (unsigned char)(random_val & 0xFF);
    }
}

// 构造 libsodium 需要的实现结构体
static randombytes_implementation stm32_rng_impl = {
    .implementation_name = NULL,
    .random = NULL,
    .stir = NULL,
    .uniform = NULL,
    .buf = stm32_randombytes_buf, // 关键：替换 buf 函数
    .close = NULL
};

// libsodium全局初始化函数
int Crypto_Init(void) {
    // 必须在 sodium_init() 之前调用！
    randombytes_set_implementation(&stm32_rng_impl);
    
    if (sodium_init() < 0) {
        return -1; // 初始化失败
    }
    return 0; // 初始化成功
}