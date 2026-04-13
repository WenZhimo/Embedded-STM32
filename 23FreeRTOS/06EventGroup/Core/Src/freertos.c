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
#include "event_groups.h"
#include <stdint.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
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
uint32_t adc_value = 0;

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
/* Definitions for BinarySem_DataReady */
osSemaphoreId_t BinarySem_DataReadyHandle;
const osSemaphoreAttr_t BinarySem_DataReady_attributes = {
  .name = "BinarySem_DataReady"
};
/* Definitions for CountingSem_Table */
osSemaphoreId_t CountingSem_TableHandle;
const osSemaphoreAttr_t CountingSem_Table_attributes = {
  .name = "CountingSem_Table"
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

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of BinarySem_DataReady */
  BinarySem_DataReadyHandle = osSemaphoreNew(1, 0, &BinarySem_DataReady_attributes);

  /* creation of CountingSem_Table */
  CountingSem_TableHandle = osSemaphoreNew(5, 5, &CountingSem_Table_attributes);

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

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Create the event(s) */
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

  lcd_dma2d_show_eubf_str(0, 32,(char*)"ADC Value:", "BoutiqueBitmap7x7_Circle_Dot", 30, WHITE);
  // 等待应用启动完成
  xEventGroupSync(xAppStartEventGroupHandle,
                  1 << 0, // APP_START_EVENT_SHOW_ADC_READY
                  (1 << 0)|(1 << 1), // APP_START_EVENT_SHOW_ADC_READY | APP_START_EVENT_SHOW_CHECKIN_READY
                  portMAX_DELAY);
  /* Infinite loop */
  for(;;)
  {
    // 等待数据准备就绪
    if(xSemaphoreTake(BinarySem_DataReadyHandle, portMAX_DELAY) == pdTRUE){
      // 数据已准备好，继续执行
      // 显示ADC值
      char adc_str[16];
      // 格式化输出，保留4位
      snprintf(adc_str, sizeof(adc_str), "%04lu", adc_value);
      // 转换为RGB565颜色
      uint16_t rgb565 = (uint16_t)((((adc_value >> 8) & 0xF) * 0x21 << 10) | (((adc_value >> 4) & 0xF) * 0x44 << 4) | ((adc_value & 0xF) * 0x21 >> 4));
      
      lcd_dma2d_fill(0, 32, 160, 65, BLACK);
      lcd_dma2d_show_eubf_str(0, 64,(char*)adc_str, "BoutiqueBitmap7x7_Circle_Dot", 32, rgb565);
      lcd_dma2d_update_screen();
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

  osDelay(1500); 

  sprintf(temp_str, "%lu", uxSemaphoreGetCount(CountingSem_TableHandle));
  
  // 等待应用启动完成
  xEventGroupSync(xAppStartEventGroupHandle,
                  1 << 1, // APP_START_EVENT_SHOW_CHECKIN_READY
                  (1 << 0)|(1 << 1), // APP_START_EVENT_SHOW_ADC_READY | APP_START_EVENT_SHOW_CHECKIN_READY
                  portMAX_DELAY);
  // 第一行：Y=32 (0~32)
  lcd_dma2d_show_eubf_str(160, 32, (char*)"TOTAL_TABLE:", "BoutiqueBitmap7x7_Scan_Line", 22, YELLOW);
  // 第二行：Y=64 (32~64)
  lcd_dma2d_show_eubf_str(160, 64, temp_str, "BoutiqueBitmap7x7_Scan_Line", 32, YELLOW);
  // 第三行：Y=96 (64~96)
  lcd_dma2d_show_eubf_str(160, 96, (char*)"AVAILABLE:", "BoutiqueBitmap7x7_Scan_Line", 24, RED);
  
  // 第一次推送到屏幕
  lcd_dma2d_update_screen();

  /* Infinite loop */
  for(;;)
  {
    
    // 按键处理
    if(myGetKeyPressStateByID(KEY_UP)){
      // 清除第五行 (占据 128~160)
      lcd_dma2d_fill(160, 128, 320, 160+1, BLACK); 
      
      if(xSemaphoreTake(CountingSem_TableHandle, 100) == pdTRUE){
        lcd_dma2d_show_eubf_str(160, 160, (char*)"CheckIn OK", "BoutiqueBitmap7x7_Scan_Line", 24, GREEN);
      } else {
        lcd_dma2d_show_eubf_str(160, 160, (char*)"CheckIn Failed", "BoutiqueBitmap7x7_Scan_Line", 24, RED);
      }
      lcd_dma2d_update_screen();
    }

    // 按需刷新
    current_count = uxSemaphoreGetCount(CountingSem_TableHandle);
    if(current_count != last_count)
    {
      sprintf(temp_str, "%lu", (unsigned long)current_count);
      
      // 清除第四行 (占据 96~128)
      lcd_dma2d_fill(160, 96, 320, 128+1, BLACK); 
      // 绘制第四行数字
      lcd_dma2d_show_eubf_str(160, 128, temp_str, "BoutiqueBitmap7x7_Scan_Line", 32, current_count?GREEN:RED);
      
      lcd_dma2d_update_screen();
      last_count = current_count;
    }
    
    osDelay(100);
    

  }
  /* USER CODE END App_Task_CheckIn */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
  if(hadc->Instance == ADC1){
    adc_value = HAL_ADC_GetValue(hadc);
    //printf("ADC Conversion Completed!\r\n");
    BaseType_t highter_priority_task_woken = pdFALSE;
    if(BinarySem_DataReadyHandle != NULL){// 确保信号量已创建
      // 释放信号量，通知显示任务数据已准备好
      xSemaphoreGiveFromISR(BinarySem_DataReadyHandle, &highter_priority_task_woken);
      portYIELD_FROM_ISR(highter_priority_task_woken);
    }
    

  }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc){
  // 这里可以添加定时器事件处理代码
  //释放计数信号量
  if(CountingSem_TableHandle != NULL){ // 确保信号量已创建
    BaseType_t highter_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(CountingSem_TableHandle, &highter_priority_task_woken);
    portYIELD_FROM_ISR(highter_priority_task_woken);
  }
}
/* USER CODE END Application */

