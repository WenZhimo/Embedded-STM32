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
#include <stdio.h>
#include <string.h>
#include "../../ThirdParty/MyLib/LED/LED.h"
#include "../../ThirdParty/MyLib/LCD/lcd.h"
#include "../../ThirdParty/MyLib/RC522/rc522.h"
#include "../../ThirdParty/MyLib/RC522/ndef.h"
#include "usart.h"
#include "rtc.h"
#include "../../ThirdParty/MyLib/CLI/cli.h"

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
/* Definitions for Task_RC522 */
osThreadId_t Task_RC522Handle;
const osThreadAttr_t Task_RC522_attributes = {
  .name = "Task_RC522",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for Task_usart1 */
osThreadId_t Task_usart1Handle;
const osThreadAttr_t Task_usart1_attributes = {
  .name = "Task_usart1",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void AppTask_LEDR(void *argument);
void AppTask_LEDG(void *argument);
void AppTask_RC522(void *argument);
void AppTask_usart1(void *argument);

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

  /* creation of Task_RC522 */
  Task_RC522Handle = osThreadNew(AppTask_RC522, NULL, &Task_RC522_attributes);

  /* creation of Task_usart1 */
  Task_usart1Handle = osThreadNew(AppTask_usart1, NULL, &Task_usart1_attributes);

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
    // LEDR_Toggle();
    // lcd_fill(0, 100, 20, 120, !HAL_GPIO_ReadPin(LEDR_GPIO_Port, LEDR_Pin)?RED:BLACK);
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
    // LEDG_Toggle();
    // lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    // osDelay(100);
    // LEDG_Toggle();
    // lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    // osDelay(100);
    // LEDG_Toggle();
    // lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
    // osDelay(100);
    // LEDG_Toggle();
    // lcd_fill(40, 100, 60, 120, !HAL_GPIO_ReadPin(LEDG_GPIO_Port, LEDG_Pin)?GREEN:BLACK);
     vTaskDelayUntil( &xLastWakeTime, xFrequency );
    //osDelayUntil(xLastWakeTime += xFrequency);//这是CMSIS2的对应封装函数
  }
  /* USER CODE END AppTask_LEDG */
}

/* USER CODE BEGIN Header_AppTask_RC522 */
/**
* @brief Function implementing the Task_RC522 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AppTask_RC522 */
void AppTask_RC522(void *argument)
{
  /* USER CODE BEGIN AppTask_RC522 */
  /** ============================= 初始化RC522 ============================= */
  // 硬件初始化后，复位射频模块并打开天线
  PcdReset();          // 软复位
  HAL_Delay(50);       // 留出足够的芯片启动时间

  PcdAntennaOff();     // 先关天线
  HAL_Delay(10);       
  PcdAntennaOn();      // 再开天线 (必须有这个动作，否则不发射电磁波)
  unsigned char version = ReadRawRC(VersionReg); // 读取版本号寄存器
  printf("RC522 Version: 0x%02X\r\n", version);
  PcdReset(); // 再复位一次，确保配置生效
  osDelay(50); // 给芯片启动留一点时间
  PcdAntennaOff(); 
  osDelay(10);
  PcdAntennaOn();  // 开启射频天线
  osDelay(50);

  uint8_t CardType[2];
  uint8_t CardUID[4];
  int8_t status;
  /* Infinite loop */
  for(;;)
  {
    // 1. 寻卡 (检测是否刷卡)
    if (PcdRequest(PICC_REQALL, CardType) == MI_OK)
    {
        // 2. 防冲撞 (读取唯一的 4 字节卡号)
        if (PcdAnticoll(CardUID) == MI_OK)
        {
            // 在这里处理读取到的卡号！
            // 比如，打印卡号：printf("Card UID: %02X %02X %02X %02X\r\n", CardUID[0], CardUID[1], CardUID[2], CardUID[3]);
            printf("Card UID: %02X %02X %02X %02X\r\n", CardUID[0], CardUID[1], CardUID[2], CardUID[3]);
            
            // 格式化卡片为NDEF空白卡
            status=NDEF_FormatUniversal(CardUID);
            if (status != MI_OK) {
                printf("Failed to Format Card. Status Code: %d\r\n", status);
            }else {
                printf("Card Formatted Successfully!\r\n");
            }
            M1_ForceWakeUp(CardUID);
            
            NDEF_Message myNdefMsg;

            // 1. 初始化空消息
            NDEF_Message_Init(&myNdefMsg);

            // 2. 添加第一条记录：让手机弹窗打开网页
            NDEF_AddUriRecord(&myNdefMsg, URI_PREFIX_HTTPS_WWWDOT, "wenzhimo.xyz");

            // 3. 添加第二条记录：传递额外的文本信息
            NDEF_AddTextRecord(&myNdefMsg, "zh", "你好，世界！这卡片支持多条NDEF。");

            NDEF_AddTextRecord(&myNdefMsg, "zh", "如你所见，这些记录时在单片机上生成并写入的。");
            
            NDEF_AddTextRecord(&myNdefMsg, "zh", "想要更多记录？");
            NDEF_AddTextRecord(&myNdefMsg, "zh", "更多记录？");
            NDEF_AddTextRecord(&myNdefMsg, "zh", "更多？");

            // 4. 写入卡片 (会自动把上述两条记录打包并计算好头部 MB/ME 标志)
            status = NDEF_WriteToCard(CardUID, &myNdefMsg);
            
            if (status == MI_OK) {
                printf("NDEF Message Written Successfully!\r\n");
            } else {
                printf("Failed to Write NDEF Message. Status Code: %d\r\n", status);
            }

            // 操作完成后，让卡片休眠，防止一直重复读取
            PcdHalt(); 
  
        }
    }
    
    
    osDelay(1000); // 延时防抖
  }
  /* USER CODE END AppTask_RC522 */
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

  /* Infinite loop */
  for(;;)
  {
    if(rx_buffer_1_flag){
        //RTC测试
        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};
        //设置RTC的时间
        int year, month, date, hour, min, sec;

        // 先读 Time，此时硬件为了保证时间一致性，会把日期寄存器“锁住”。
        // 只有当你紧接着调用 HAL_RTC_GetDate() 读取日期后，寄存器才会“解锁”。
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        // 紧接着必须读 Date (即使你不用 Date 的数据，也必须调用此函数解锁硬件)
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        //不过当你只需要date时，只调用HAL_RTC_GetDate并不会“锁住”日期寄存器。
        // 3. 打印时间 (注意我们之前在 CMake 里解决的 printf 缓冲和结束符问题)
        printf("Time: %02d:%02d:%02d, Date: 20%02d-%02d-%02d\r\n", 
               sTime.Hours, sTime.Minutes, sTime.Seconds,
               sDate.Year, sDate.Month, sDate.Date);
        // 1. 数据处理
        
        //printf("Received: %s", rx_buffer_1);
        //HAL_UART_Transmit(&huart1, rx_buffer_1, Size, 100);
        HAL_GPIO_TogglePin(LEDR_GPIO_Port, LEDR_Pin); // 接收指令时切换 LED 灯状态，方便观察

        // 尝试匹配 "SET_TIME:年-月-日-时-分-秒" 格式
        if (sscanf((char *)rx_buffer_1, "SET_TIME:%d-%d-%d-%d-%d-%d", 
                    &year, &month, &date, &hour, &min, &sec) == 6)
        {
            // 如果成功匹配到 6 个参数，说明格式正确，调用设时函数
            Update_RTC_Time(year, month, date, hour, min, sec);
            printf("RTC Time Synced Successfully!\r\n");

            // HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        
            // HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
            // printf("Time: %02d:%02d:%02d, Date: 20%02d-%02d-%02d\r\n", 
            //    sTime.Hours, sTime.Minutes, sTime.Seconds,
            //    sDate.Year, sDate.Month, sDate.Date);
        }
        else
        {
            // 格式不匹配，当作普通指令处理
            printf("Unknown Command: %s\r\n", rx_buffer_1);
        }
        memset(rx_buffer_1, 0, MAX_CMD_LEN); // 清空已处理的数据，准备接收下一帧指令

        // 2. 处理完成后，清除标志位，准备接收下一条指令
        rx_buffer_1_flag = 0;
    }
    osDelay(1);
  }
  /* USER CODE END AppTask_usart1 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

