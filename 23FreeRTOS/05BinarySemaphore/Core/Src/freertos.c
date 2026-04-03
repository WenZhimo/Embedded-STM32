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
#include "portmacro.h"
#include "stm32f4xx_hal_adc.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../ThirdParty/MyLib/myinclude.h"
#include "adc.h"
#include "semphr.h"
#include <stdint.h>
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
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityRealtime7,
};
/* Definitions for Task_showADC */
osThreadId_t Task_showADCHandle;
const osThreadAttr_t Task_showADC_attributes = {
  .name = "Task_showADC",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for BinarySem_DataReady */
osSemaphoreId_t BinarySem_DataReadyHandle;
const osSemaphoreAttr_t BinarySem_DataReady_attributes = {
  .name = "BinarySem_DataReady"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void AppTask_SYS_INIT(void *argument);
void App_Task_showADC(void *argument);

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

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

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
void AppTask_SYS_INIT(void *argument)
{
  /* USER CODE BEGIN AppTask_SYS_INIT */
  /* Infinite loop */
  for(;;)
  {
    SD_Mount_StateMachine();
    if(SD_Is_Mounted() == 1){
      //printf("SD Card Mounted Successfully!\r\n");
      
      lcd_dma2d_show_eubf_str(0, 32, (char*)"诸行无常，是生灭法", "字酷堂板桥体", 32, WHITE);
      lcd_dma2d_update_screen();
      vTaskDelete(NULL);
    }
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
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END App_Task_showADC */
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
/* USER CODE END Application */

