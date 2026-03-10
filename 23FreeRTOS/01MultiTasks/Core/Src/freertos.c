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
#include "../../ThirdParty/MyLib/LED/LED.h"
#include "../../ThirdParty/MyLib/LCD/lcd.h"
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

/* USER CODE END Variables */
/* Definitions for Task_LEDR */
osThreadId_t Task_LEDRHandle;
const osThreadAttr_t Task_LEDR_attributes = {
  .name = "Task_LEDR",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for Task_LEDG */
osThreadId_t Task_LEDGHandle;
uint32_t Task_LED1Buffer[ 128 ];
osStaticThreadDef_t Task_LED1ControlBlock;
const osThreadAttr_t Task_LEDG_attributes = {
  .name = "Task_LEDG",
  .cb_mem = &Task_LED1ControlBlock,
  .cb_size = sizeof(Task_LED1ControlBlock),
  .stack_mem = &Task_LED1Buffer[0],
  .stack_size = sizeof(Task_LED1Buffer),
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void AppTask_LEDR(void *argument);
void AppTask_LEDG(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

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
  /* creation of Task_LEDR */
  Task_LEDRHandle = osThreadNew(AppTask_LEDR, NULL, &Task_LEDR_attributes);

  /* creation of Task_LEDG */
  Task_LEDGHandle = osThreadNew(AppTask_LEDG, NULL, &Task_LEDG_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_AppTask_LEDR */
/**
  * @brief  Function implementing the Task_LEDR thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_AppTask_LEDR */
void AppTask_LEDR(void *argument)
{
  /* USER CODE BEGIN AppTask_LEDR */
  /* Infinite loop */
  for(;;)
  {
    LEDR_Toggle();
    lcd_fill(0, 100, 20, 120, !HAL_GPIO_ReadPin(LEDR_GPIO_Port, LEDR_Pin)?RED:BLACK);
    osDelay(500);
  }
  /* USER CODE END AppTask_LEDR */
}

/* USER CODE BEGIN Header_AppTask_LEDG */
/**
* @brief Function implementing the Task_LEDG thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AppTask_LEDG */
void AppTask_LEDG(void *argument)
{
  /* USER CODE BEGIN AppTask_LEDG */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS( 1000 );
  /* Infinite loop */
  for(;;)
  {
    LEDG_Toggle();
    lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    osDelay(100);
    LEDG_Toggle();
    lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    osDelay(100);
    LEDG_Toggle();
    lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    osDelay(100);
    LEDG_Toggle();
    lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    vTaskDelayUntil( &xLastWakeTime, xFrequency );
    //osDelayUntil(xLastWakeTime += xFrequency);//这是CMSIS2的对应封装函数
  }
  /* USER CODE END AppTask_LEDG */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

