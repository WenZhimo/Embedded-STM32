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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../ThirdParty/MyLib/LED/LED.h"
#include "../../ThirdParty/MyLib/LCD/lcd.h"
#include "usart.h"
#include "rtc.h"
#include "../../ThirdParty/MyLib/CLI/cli.h"
#include "../../ThirdParty/MyLib/CLI/cli_fun.h"
#include "../../ThirdParty/MyLib/USB/usb_app.h"
extern ApplicationTypeDef Appli_state;
#include "../../ThirdParty/MyLib/EUBF/eubf_manager.h"
#include "../../ThirdParty/MyLib/LCD/lcd_dma2d.h"
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
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Task_usart1 */
osThreadId_t Task_usart1Handle;
uint32_t Task_usart1Buffer[ 1024 ];
osStaticThreadDef_t Task_usart1ControlBlock;
const osThreadAttr_t Task_usart1_attributes = {
  .name = "Task_usart1",
  .cb_mem = &Task_usart1ControlBlock,
  .cb_size = sizeof(Task_usart1ControlBlock),
  .stack_mem = &Task_usart1Buffer[0],
  .stack_size = sizeof(Task_usart1Buffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for Task_LCDTime */
osThreadId_t Task_LCDTimeHandle;
uint32_t Task_LCDTimeBuffer[ 256 ];
osStaticThreadDef_t Task_LCDTimeControlBlock;
const osThreadAttr_t Task_LCDTime_attributes = {
  .name = "Task_LCDTime",
  .cb_mem = &Task_LCDTimeControlBlock,
  .cb_size = sizeof(Task_LCDTimeControlBlock),
  .stack_mem = &Task_LCDTimeBuffer[0],
  .stack_size = sizeof(Task_LCDTimeBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for TaskInitUSB */
osThreadId_t TaskInitUSBHandle;
uint32_t TaskUSBBuffer[ 512 ];
osStaticThreadDef_t TaskUSBControlBlock;
const osThreadAttr_t TaskInitUSB_attributes = {
  .name = "TaskInitUSB",
  .cb_mem = &TaskUSBControlBlock,
  .cb_size = sizeof(TaskUSBControlBlock),
  .stack_mem = &TaskUSBBuffer[0],
  .stack_size = sizeof(TaskUSBBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for TaskreadUSB */
osThreadId_t TaskreadUSBHandle;
uint32_t TaskreadUSBBuffer[ 512 ];
osStaticThreadDef_t TaskreadUSBControlBlock;
const osThreadAttr_t TaskreadUSB_attributes = {
  .name = "TaskreadUSB",
  .cb_mem = &TaskreadUSBControlBlock,
  .cb_size = sizeof(TaskreadUSBControlBlock),
  .stack_mem = &TaskreadUSBBuffer[0],
  .stack_size = sizeof(TaskreadUSBBuffer),
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void AppTask_LEDR(void *argument);
void AppTask_usart1(void *argument);
void AppTask_LCDTime(void *argument);
void AppTaskInitUSB(void *argument);
void AppTaskreadUSB(void *argument);

extern void MX_USB_HOST_Init(void);
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

  /* creation of Task_usart1 */
  Task_usart1Handle = osThreadNew(AppTask_usart1, NULL, &Task_usart1_attributes);

  /* creation of Task_LCDTime */
  Task_LCDTimeHandle = osThreadNew(AppTask_LCDTime, NULL, &Task_LCDTime_attributes);

  /* creation of TaskInitUSB */
  TaskInitUSBHandle = osThreadNew(AppTaskInitUSB, NULL, &TaskInitUSB_attributes);

  /* creation of TaskreadUSB */
  TaskreadUSBHandle = osThreadNew(AppTaskreadUSB, NULL, &TaskreadUSB_attributes);

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
  /* init code for USB_HOST */
  MX_USB_HOST_Init();
  /* USER CODE BEGIN AppTask_LEDR */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END AppTask_LEDR */
}

/* USER CODE BEGIN Header_AppTask_usart1 */
/**
* @brief Function implementing the Task_usart1 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AppTask_usart1 */
void AppTask_usart1(void *argument)
{
  /* USER CODE BEGIN AppTask_usart1 */
  cli_t cli_USART1;
  cli_cmd_entry_t cli_USART1_cmd_entry[] = {
    {"LED", cli_led},
    {"SET_TIME", cli_rtc},
    {"LCD", cli_lcd},
  };
  CLI_Init(&cli_USART1, 
    cli_USART1_cmd_entry,
    sizeof(cli_USART1_cmd_entry)/sizeof(cli_USART1_cmd_entry[0]));

  /* Infinite loop */
  for(;;)
  {
    if(rx_buffer_1_flag){
        
        // 1. 数据处理
        
        //printf("Received: %s", rx_buffer_1);
        //HAL_UART_Transmit(&huart1, rx_buffer_1, Size, 100);
        //LEDG_Toggle(); // 接收指令时切换 LED 灯状态，方便观察
        // 2. 指令解析和执行
        CLI_Input(&cli_USART1, (const char *)rx_buffer_1,rx_buffer_1_flag);
        
        memset(rx_buffer_1, 0, MAX_CMD_LEN); // 清空已处理的数据，准备接收下一帧指令

        // 2. 处理完成后，清除标志位，准备接收下一条指令
        rx_buffer_1_flag = 0;
    }
    osDelay(50);
  }
  /* USER CODE END AppTask_usart1 */
}

/* USER CODE BEGIN Header_AppTask_LCDTime */
/**
* @brief Function implementing the Task_LCDTime thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AppTask_LCDTime */
void AppTask_LCDTime(void *argument)
{
  /* USER CODE BEGIN AppTask_LCDTime */
  /* Infinite loop */
  for(;;)
  {
    // //RTC测试
    // RTC_TimeTypeDef sTime = {0};
    // RTC_DateTypeDef sDate = {0};
    // //设置RTC的时间
    
    // char RTCbuffer[128];
    // // 先读 Time，此时硬件为了保证时间一致性，会把日期寄存器“锁住”。
    // // 只有当你紧接着调用 HAL_RTC_GetDate() 读取日期后，寄存器才会“解锁”。
    // HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    // // 紧接着必须读 Date (即使你不用 Date 的数据，也必须调用此函数解锁硬件)
    // HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    // //不过当你只需要date时，只调用HAL_RTC_GetDate并不会“锁住”日期寄存器。
    // // 3. 打印时间 (注意我们之前在 CMake 里解决的 printf 缓冲和结束符问题)
    // sprintf(RTCbuffer, "Time: %02d:%02d:%02d,\nDate: 20%02d-%02d-%02d", 
    //         sTime.Hours, sTime.Minutes, sTime.Seconds,
    //         sDate.Year, sDate.Month, sDate.Date);
    // lcd_show_string(0, 120,300,65,32, RTCbuffer, WHITE);
    osDelay(5000);
  }
  /* USER CODE END AppTask_LCDTime */
}

/* USER CODE BEGIN Header_AppTaskInitUSB */
/**
* @brief Function implementing the TaskInitUSB thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AppTaskInitUSB */
void AppTaskInitUSB(void *argument)
{
  /* USER CODE BEGIN AppTaskInitUSB */
  /* Infinite loop */
  // 直接调用我们封装好的后台管理线程！
  // 这个函数内部自带死循环和 osDelay，会永远在这里面运行并管理 U 盘的拔插。
  USB_App_Thread(argument);
  /* USER CODE END AppTaskInitUSB */
}

/* USER CODE BEGIN Header_AppTaskreadUSB */
/**
* @brief Function implementing the TaskreadUSB thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AppTaskreadUSB */
void AppTaskreadUSB(void *argument)
{
  /* USER CODE BEGIN AppTaskreadUSB */
   EUBF_Init();
    uint8_t text_buffer[512];
    uint32_t br;
    uint8_t first_draw = 0;
    

    /* 1. 定义一个 TCHAR 缓冲区用于存放转换后的文件名 */
    TCHAR w_filename[32]; 
    /* 2. 转换文件名 */
    USB_Utils_UTF8ToUTF16("usbtest.txt", w_filename, 32);

    for(;;)
    {
        if ( USB_Is_Mounted() && first_draw == 0)
        {
          //printf("1. usb mounted\r\n");
            /* 3. 使用转换后的 TCHAR 缓冲区调用接口 */
            if (USB_Read_File(w_filename, text_buffer, sizeof(text_buffer) - 1, &br) == 0)
            {
                
                text_buffer[br] = '\0'; 
                //printf("2. file read success,read: %s\r\n", text_buffer);

                //uint16_t test_h = EUBF_Get_Line_Height("峄山碑篆体", 32);
                //printf("[Debug] Font Line Height: %d\r\n", test_h);
                
                lcd_dma2d_clear(BLACK); 
                // 显示文件内容
                lcd_dma2d_show_eubf_str(0, 50, (char*)text_buffer, 
                                    "峄山碑篆体", 32, WHITE);
                // 显示测试字符串
                lcd_dma2d_show_eubf_str(0, 100, (char*)"This is a test.",
                                    "Megrim", 32, WHITE);
                                    
                lcd_dma2d_show_eubf_str(0, 150, (char*)"诸行无常 是生灭法", 
                                    "字酷堂板桥体", 32, WHITE);
                lcd_dma2d_show_eubf_str(0, 200, (char*)"三十岁梦忆，五十岁复梦忆，\n所梦或同  ，  所忆错落矣。", 
                                    "华康金文体", 25, WHITE);

                //printf("4. Calling Update Screen...\r\n");
                lcd_dma2d_update_screen();
                first_draw = 1;
                
            }
        }
        osDelay(5000); 
    }
  /* USER CODE END AppTaskreadUSB */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

