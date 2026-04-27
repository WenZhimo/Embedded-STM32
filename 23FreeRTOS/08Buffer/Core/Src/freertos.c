/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../ThirdParty/MyLib/myinclude.h"
#include "rtc.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "message_buffer.h"
#include "event_groups.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include <sys/types.h>
#include "sodium.h"
#include "string.h"
#include "rng.h"



/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern RNG_HandleTypeDef hrng;
// 加密数据流缓冲区句柄
StreamBufferHandle_t xCryptoStreamBuffer = NULL;
MessageBufferHandle_t xKeyMessageBuffer = NULL;
/* USER CODE END Variables */
/* Definitions for Task_SYS_INIT */
osThreadId_t Task_SYS_INITHandle;
const osThreadAttr_t Task_SYS_INIT_attributes = {
  .name = "Task_SYS_INIT",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for Task_showADC */
osThreadId_t Task_showADCHandle;
const osThreadAttr_t Task_showADC_attributes = {
  .name = "Task_showADC",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for Task_CheckIn */
osThreadId_t Task_CheckInHandle;
const osThreadAttr_t Task_CheckIn_attributes = {
  .name = "Task_CheckIn",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for Task_Crypto */
osThreadId_t Task_CryptoHandle;
uint32_t Task_CryptoBuffer[ 256 ];
osStaticThreadDef_t Task_CryptoControlBlock;
const osThreadAttr_t Task_Crypto_attributes = {
  .name = "Task_Crypto",
  .cb_mem = &Task_CryptoControlBlock,
  .cb_size = sizeof(Task_CryptoControlBlock),
  .stack_mem = &Task_CryptoBuffer[0],
  .stack_size = sizeof(Task_CryptoBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DMA2D_Mutex */
osMutexId_t DMA2D_MutexHandle;
const osMutexAttr_t DMA2D_Mutex_attributes = {
  .name = "DMA2D_Mutex"
};
/* Definitions for xSystemInitEventGroup */
osEventFlagsId_t xSystemInitEventGroupHandle;
const osEventFlagsAttr_t xSystemInitEventGroup_attributes = {
  .name = "xSystemInitEventGroup"
};
/* Definitions for xAppStartEventGroup */
osEventFlagsId_t xAppStartEventGroupHandle;
const osEventFlagsAttr_t xAppStartEventGroup_attributes = {
  .name = "xAppStartEventGroup"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void AppTask_SYS_INIT(void *argument);
void App_Task_showADC(void *argument);
void App_Task_CheckIn(void *argument);
void AppTask_Crypto(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  //创建加密数据流缓冲区
  xCryptoStreamBuffer = xStreamBufferCreate(1024, 1);
  //创建按键消息缓冲区
  xKeyMessageBuffer = xMessageBufferCreate(256);
  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of DMA2D_Mutex */
  DMA2D_MutexHandle = osMutexNew(&DMA2D_Mutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Task_SYS_INIT */
  Task_SYS_INITHandle = osThreadNew(AppTask_SYS_INIT, NULL, &Task_SYS_INIT_attributes);

  /* creation of Task_showADC */
  Task_showADCHandle = osThreadNew(App_Task_showADC, NULL, &Task_showADC_attributes);

  /* creation of Task_CheckIn */
  Task_CheckInHandle = osThreadNew(App_Task_CheckIn, NULL, &Task_CheckIn_attributes);

  /* creation of Task_Crypto */
  Task_CryptoHandle = osThreadNew(AppTask_Crypto, NULL, &Task_Crypto_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of xSystemInitEventGroup */
  xSystemInitEventGroupHandle = osEventFlagsNew(&xSystemInitEventGroup_attributes);

  /* creation of xAppStartEventGroup */
  xAppStartEventGroupHandle = osEventFlagsNew(&xAppStartEventGroup_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_AppTask_SYS_INIT */
/**
  * @brief  Function implementing the Task_SYS_INIT thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_AppTask_SYS_INIT */
__weak void AppTask_SYS_INIT(void *argument)
{
  /* USER CODE BEGIN AppTask_SYS_INIT */
  /* Infinite loop */
  for(;;)
  {
    
    osDelay(1);
  }
  /* USER CODE END AppTask_SYS_INIT */
}

/* USER CODE BEGIN Header_App_Task_showADC */
/**
* @brief Function implementing the Task_showADC thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_Task_showADC */
void App_Task_showADC(void *argument)
{
  /* USER CODE BEGIN App_Task_showADC */
  //等待ADC初始化完成、定时器3初始化完成、SD卡挂载完成、EUBF初始化完成、DMA2D初始化完成、LCD初始化完成
  xEventGroupWaitBits(xSystemInitEventGroupHandle, 
                        SYSTEM_INIT_EVENT_ADC_READY | 
                        SYSTEM_INIT_EVENT_TIM3_READY | 
                        SYSTEM_INIT_EVENT_SD_MOUNTED | 
                        SYSTEM_INIT_EVENT_EUBF_READY | 
                        SYSTEM_INIT_EVENT_DMA2D_READY | 
                        SYSTEM_INIT_EVENT_LCD_READY, 
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

  xSemaphoreTake(DMA2D_MutexHandle, portMAX_DELAY);
  lcd_dma2d_show_eubf_str(0, 32,(char*)"ADC Value:", "BoutiqueBitmap7x7_Circle_Dot", 30, WHITE);
  lcd_dma2d_update_screen();
  xSemaphoreGive(DMA2D_MutexHandle);
  uint32_t show_adc_value = 0;
  // 等待应用启动完成
  xEventGroupSync(xAppStartEventGroupHandle,
                  1 << 0, // APP_START_EVENT_SHOW_ADC_READY
                  (1 << 0)|(1 << 1), // APP_START_EVENT_SHOW_ADC_READY | APP_START_EVENT_SHOW_CHECKIN_READY
                  portMAX_DELAY);
  /* Infinite loop */
  for(;;)
  {
    // 等待数据准备就绪
    if(xTaskNotifyWait(0, 0XFFFFFFFF, &show_adc_value, portMAX_DELAY) == pdTRUE){
      
      xSemaphoreTake(DMA2D_MutexHandle, portMAX_DELAY); // 先拿锁
      
      if(show_adc_value == 0xFFFFFFFF) {
          // --- 处理错误情况 ---
          static uint8_t error_count = 0;
          error_count++;
          char adc_err_str[30];
          snprintf(adc_err_str, sizeof(adc_err_str), "ADC Error: %d", error_count);
          
          lcd_dma2d_fill(0, 96, 160, 128, BLACK); 
          lcd_dma2d_show_eubf_str(0, 96, adc_err_str, "BoutiqueBitmap7x7_Circle_Dot", 32, RED);
      } else {
          // --- 处理正常情况 ---
          char adc_str[20];
          snprintf(adc_str, sizeof(adc_str), "%04lu", show_adc_value);
          uint16_t rgb565 = (uint16_t)((((show_adc_value >> 8) & 0xF) * 0x21 << 10) | (((show_adc_value >> 4) & 0xF) * 0x44 << 4) | ((show_adc_value & 0xF) * 0x21 >> 4));
          
          lcd_dma2d_fill(0, 32, 160, 65, BLACK);
          lcd_dma2d_show_eubf_str(0, 64, adc_str, "BoutiqueBitmap7x7_Circle_Dot", 32, rgb565);
      }
      
      lcd_dma2d_update_screen();
      xSemaphoreGive(DMA2D_MutexHandle); // 后放锁
      
      osDelay(1);
    }
    osDelay(1);
  }
  /* USER CODE END App_Task_showADC */
}

/* USER CODE BEGIN Header_App_Task_CheckIn */
/**
* @brief Function implementing the Task_CheckIn thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_Task_CheckIn */
void App_Task_CheckIn(void *argument)
{
  /* USER CODE BEGIN App_Task_CheckIn */
  //等待SD卡挂载完成、EUBF初始化完成、DMA2D初始化完成、LCD初始化完成
  xEventGroupWaitBits(xSystemInitEventGroupHandle, 
                        SYSTEM_INIT_EVENT_SD_MOUNTED | 
                        SYSTEM_INIT_EVENT_EUBF_READY | 
                        SYSTEM_INIT_EVENT_DMA2D_READY | 
                        SYSTEM_INIT_EVENT_LCD_READY, 
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

  
    char temp_str[30];
    UBaseType_t last_count = 0xFFFFFFFF; 
    UBaseType_t current_count = 0;
    uint16_t latest_msg = 0;        // 记录最新一条消息内容
    uint16_t last_msg    = 0xFFFF;   // 追踪 latest_msg 变化以刷新二进制行

    osDelay(1500);  

    // 等待应用启动完成
    xEventGroupSync(xAppStartEventGroupHandle,
                    1 << 1, // APP_START_EVENT_SHOW_CHECKIN_READY
                    (1 << 0)|(1 << 1), // APP_START_EVENT_SHOW_ADC_READY | APP_START_EVENT_SHOW_CHECKIN_READY
                    portMAX_DELAY);
    xSemaphoreTake(DMA2D_MutexHandle, portMAX_DELAY);
    // 第一行：Y=32 (0~32)
    lcd_dma2d_show_eubf_str(160, 32, (char*)"TOTAL_TABLE:", "BoutiqueBitmap7x7_Scan_Line", 22, YELLOW);
    // 第二行：Y=64 (32~64)
    lcd_dma2d_show_eubf_str(160, 64, "无限!∞!", "BoutiqueBitmap7x7_Scan_Line", 32, GREEN);
    // 第三行：Y=96 (64~96)
    lcd_dma2d_show_eubf_str(160, 96, (char*)"AVAILABLE:", "BoutiqueBitmap7x7_Scan_Line", 24, RED);
  
    // 第一次推送到屏幕
    lcd_dma2d_update_screen();
    xSemaphoreGive(DMA2D_MutexHandle);
  
        /* Infinite loop */
    for(;;)
    {
      // ==========================================
      // 实时读取 ISR 累积的任务通知增量（非阻塞）
      // ==========================================
      {
        uint32_t notify_increment;
        if(xTaskNotifyWait(0, 0xFFFFFFFF, &notify_increment, 0) == pdTRUE){
          current_count += notify_increment;
        }
      }

      // ==========================================
      // 实时从消息缓冲区收取数据（非阻塞），只保留最新值
      // ==========================================
      {
        uint16_t rtc_msg;
        while(xMessageBufferReceive(xKeyMessageBuffer, &rtc_msg, sizeof(rtc_msg), 0) > 0){
          latest_msg = rtc_msg;
        }
      }

      // ==========================================
      // 每 20ms：检查按键
      // ==========================================
      if(myGetKeyPressStateByID(KEY_UP)){
        xSemaphoreTake(DMA2D_MutexHandle, portMAX_DELAY);
        // 清除第五行 OK/FAIL 区域 (Y=160 是下边界, size=24 文字占据 Y=136~160)
        lcd_dma2d_fill(160, 128, 320, 161, BLACK);  

        if(current_count > 0)
        {
          // 签到成功
          current_count--;
          char checkin_str[30];
          snprintf(checkin_str, sizeof(checkin_str), "OK:%04X", latest_msg);
          lcd_dma2d_show_eubf_str(160, 160, checkin_str, "BoutiqueBitmap7x7_Scan_Line", 24, GREEN);
        }
        else
        {
          // 签到失败（无待签到计数）
          char fail_str[30];
          snprintf(fail_str, sizeof(fail_str), "FAIL:%04X", latest_msg);
          lcd_dma2d_show_eubf_str(160, 160, fail_str, "BoutiqueBitmap7x7_Scan_Line", 24, RED);
        }

                lcd_dma2d_update_screen();
        xSemaphoreGive(DMA2D_MutexHandle);
      }

      // ==========================================
      // latest_msg 变化立即清除旧值并刷新二进制行
      // ==========================================
      if(latest_msg != last_msg)
      {
        xSemaphoreTake(DMA2D_MutexHandle, portMAX_DELAY);

        // 清除二进制行（Y=192 下边界, size=8 占据 184~192, 多清一些确保覆盖残留像素）
        lcd_dma2d_fill(160, 176, 320, 193, BLACK);

        for(int i = 0; i < 16; i++) {
          uint8_t bit = (latest_msg >> (15 - i)) & 0x01;
          char bit_char[2] = { bit ? '1' : '0', '\0' };
          uint16_t color = bit ? GREEN : RED;
          lcd_dma2d_show_eubf_str(160 + i * 8, 192, bit_char, "3x3等宽monospace", 8, color);
        }

        lcd_dma2d_update_screen();
        xSemaphoreGive(DMA2D_MutexHandle);
        last_msg = latest_msg;
      }

      // ==========================================
      // 每次循环都检测 current_count 是否变化，变化立即刷新
      // ==========================================
      if(current_count != last_count)
      {
        sprintf(temp_str, "%lu", (unsigned long)current_count);
    
        // 清除第四行 (占据 Y=96~128)
        xSemaphoreTake(DMA2D_MutexHandle, portMAX_DELAY);
        lcd_dma2d_fill(160, 96, 320, 129, BLACK); 
        // 绘制第四行数字
        lcd_dma2d_show_eubf_str(160, 128, temp_str, "BoutiqueBitmap7x7_Scan_Line", 32, current_count?GREEN:RED);
    
        lcd_dma2d_update_screen();
        xSemaphoreGive(DMA2D_MutexHandle);
        last_count = current_count;
      }
  
      osDelay(20);
    

  }
  /* USER CODE END App_Task_CheckIn */
}


/* USER CODE BEGIN Header_AppTask_Crypto */
/**
* @brief Function implementing the Task_Crypto thread.
* @param argument: Not used
* @retval None
*/

#include "mbedtls/base64.h" // 🌟 必须包含 mbedTLS 的 Base64 头文件
#define STREAM_TIMEOUT_MS 200 // 200ms 超时视为 EOF
/* USER CODE END Header_AppTask_Crypto */
void AppTask_Crypto(void *argument)
{
  /* USER CODE BEGIN AppTask_Crypto */

  xEventGroupWaitBits(xSystemInitEventGroupHandle, 
                      SYSTEM_INIT_EVENT_SODIUM_READY | 
                      SYSTEM_INIT_EVENT_USART1_READY , 
                      pdFALSE, pdTRUE, portMAX_DELAY);

  // mbedTLS 专用：3 个明文字节编码后是 4 个字符，加上 '\0' 一共需要 5 个字节
  unsigned char b64_chunk[5]; 
  size_t olen = 0; // 用于接收 mbedTLS 返回的实际编码长度

  uint8_t block[3];
  uint8_t block_len = 0;
  uint8_t is_receiving = 0;

  /* Infinite loop */
  for(;;)
  {
    uint8_t byte;
    // 阻塞等待流缓冲区的数据，每次读取 1 个字节
    size_t xReceivedBytes = xStreamBufferReceive(
      xCryptoStreamBuffer, 
      &byte, 
      1, 
      pdMS_TO_TICKS(STREAM_TIMEOUT_MS)
    );

    // ==========================================
    // 读到了数据 
    // ==========================================
    if (xReceivedBytes > 0) 
    {
        if (!is_receiving) {
            printf("\r\n--- Streaming Base64 Encode (mbedTLS) ---\r\nText:\r\n");
            is_receiving = 1;
        }

        block[block_len] = byte;
        block_len++;

        // 凑满 3 个字节，立刻编码发走
        if (block_len == 3) {
            // 🌟 核心替换：调用 mbedTLS 的编码函数
            mbedtls_base64_encode(b64_chunk, sizeof(b64_chunk), &olen, block, 3);
            
            // mbedTLS 会自动在末尾添加 '\0'，所以可以直接用 %s 打印
            printf("%s", b64_chunk);
            block_len = 0;
        }
    }
    // ==========================================
    // 发生了超时 (200ms 内连 1 个字节都没收到) -> 数据流结束！
    // ==========================================
    else if (is_receiving) 
    {
        // 处理最后不够 3 个字节的尾巴，生成带有 "=" 的 Base64
        if (block_len > 0) {
            // 🌟 核心替换：根据实际剩余的 block_len 进行编码
            mbedtls_base64_encode(b64_chunk, sizeof(b64_chunk), &olen, block, block_len);
            printf("%s", b64_chunk);
            block_len = 0;
        }

        printf("\r\n-------------------------------\r\n");
        UBaseType_t free_stack = uxTaskGetStackHighWaterMark(NULL);
        printf("AppTask_Crypto Free Stack High Water Mark: %lu\r\n", free_stack);
        is_receiving = 0; // 状态机复位
    }
  }
  /* USER CODE END AppTask_Crypto */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
  if(hadc->Instance == ADC1){
    // adc_value = HAL_ADC_GetValue(hadc);
    //printf("ADC Conversion Completed!\r\n");
    BaseType_t highter_priority_task_woken = pdFALSE;
    if(Task_showADCHandle != NULL){ // 确保任务句柄已创建
      // 直接将ADC值作为通知值发送给显示任务
      xTaskNotifyFromISR(Task_showADCHandle, HAL_ADC_GetValue(hadc), eSetValueWithOverwrite,&highter_priority_task_woken);
      portYIELD_FROM_ISR(highter_priority_task_woken);  
    }
     
  }
}
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    if(hadc->Instance == ADC1) {
        BaseType_t highter_priority_task_woken = pdFALSE;
        
        if(Task_showADCHandle != NULL){
            // 发送 0xFFFFFFFF 代表发生错误
            xTaskNotifyFromISR(Task_showADCHandle, 0xFFFFFFFF, eSetValueWithOverwrite, &highter_priority_task_woken);
            portYIELD_FROM_ISR(highter_priority_task_woken);  
        }
        
        // 尝试清除错误并重启 ADC
        HAL_ADC_Start_IT(hadc); 
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc){
  // 发送任务通知增量 + 随机值到消息缓冲区
  if(xKeyMessageBuffer != NULL){ // 确保消息缓冲区已创建
    BaseType_t highter_priority_task_woken = pdFALSE;
    uint32_t rand_val;
    
    // 1. 将 App_Task_CheckIn 的任务通知值加 1
    if(Task_CheckInHandle != NULL){
      xTaskNotifyFromISR(Task_CheckInHandle, 1, eIncrement, &highter_priority_task_woken);
    }
    
    // 2. 用消息缓冲区发送一个随机值
    if(HAL_RNG_GenerateRandomNumber(&hrng, &rand_val) == HAL_OK){
      uint16_t msg_val = (uint16_t)(rand_val & 0xFFFF);
      xMessageBufferSendFromISR(xKeyMessageBuffer, &msg_val, sizeof(msg_val), &highter_priority_task_woken);
    }
    
    portYIELD_FROM_ISR(highter_priority_task_woken);
  }
}
/* USER CODE END Application */

