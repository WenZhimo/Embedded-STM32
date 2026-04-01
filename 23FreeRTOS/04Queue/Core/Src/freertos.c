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
#include "LED.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../ThirdParty/MyLib/myinclude.h"
#include "queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    KeyID_t    key_id;        // 哪个键
    KeyEvent_t event;         // 发生了什么事件 (短按/长按)
} KeyMessage_t;
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
/* Definitions for Task_SYS_INIT */
osThreadId_t Task_SYS_INITHandle;
const osThreadAttr_t Task_SYS_INIT_attributes = {
  .name = "Task_SYS_INIT",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityRealtime7,
};
/* Definitions for Task_usart1 */
osThreadId_t Task_usart1Handle;
const osThreadAttr_t Task_usart1_attributes = {
  .name = "Task_usart1",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Task_Draw */
osThreadId_t Task_DrawHandle;
const osThreadAttr_t Task_Draw_attributes = {
  .name = "Task_Draw",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for Task_ScanKeys */
osThreadId_t Task_ScanKeysHandle;
const osThreadAttr_t Task_ScanKeys_attributes = {
  .name = "Task_ScanKeys",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Queue_Keys */
osMessageQueueId_t Queue_KeysHandle;
const osMessageQueueAttr_t Queue_Keys_attributes = {
  .name = "Queue_Keys"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void AppTask_SYS_INIT(void *argument);
void AppTask_usart1(void *argument);
void App_Task_Draw(void *argument);
void App_Task_ScanKeys(void *argument);

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

  /* Create the queue(s) */
  /* creation of Queue_Keys */
  Queue_KeysHandle = osMessageQueueNew (16, sizeof(KeyMessage_t), &Queue_Keys_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Task_SYS_INIT */
  Task_SYS_INITHandle = osThreadNew(AppTask_SYS_INIT, NULL, &Task_SYS_INIT_attributes);

  /* creation of Task_usart1 */
  Task_usart1Handle = osThreadNew(AppTask_usart1, NULL, &Task_usart1_attributes);

  /* creation of Task_Draw */
  Task_DrawHandle = osThreadNew(App_Task_Draw, NULL, &Task_Draw_attributes);

  /* creation of Task_ScanKeys */
  Task_ScanKeysHandle = osThreadNew(App_Task_ScanKeys, NULL, &Task_ScanKeys_attributes);

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
      printf("SD Card Mounted Successfully!\r\n");
      lcd_dma2d_show_eubf_str(0, 32, (char*)"诸行无常 是生灭法", "字酷堂板桥体", 32, WHITE);
      lcd_dma2d_update_screen();
      vTaskDelete(NULL);
    }
    osDelay(1);
  }
  /* USER CODE END AppTask_SYS_INIT */
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
    osDelay(500);
  }
  /* USER CODE END AppTask_usart1 */
}

/* USER CODE BEGIN Header_App_Task_Draw */
/**
* @brief Function implementing the Task_Draw thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_Task_Draw */
void App_Task_Draw(void *argument)
{
  /* USER CODE BEGIN App_Task_Draw */
  KeyMessage_t msg;
  
  uint8_t  line_count = 0;          // 记录当前屏幕上已经打印了多少行
  uint16_t current_y = 0;           // 记录当前准备打印的 Y 坐标
  
  const uint16_t START_Y = 32;      // 第一行文字的起始 Y 坐标
  const uint16_t LINE_SPACING = 40; // 行距（字体是32，行距设为40能留出8像素的清爽空白）
  const uint16_t FONT_SIZE = 32;    // 字体大小

  // 任务开始前，先给屏幕刷个黑底，防止有杂色
  lcd_dma2d_clear(BLACK);
  lcd_dma2d_update_screen();

  /* Infinite loop */
  for(;;)
  {
    // 阻塞等待队列消息
    if (xQueueReceive(Queue_KeysHandle, &msg, portMAX_DELAY) == pdTRUE)
    {
        // 判断是否需要清屏（满 9 条则清空）
        if (line_count >= 9) 
        {
            lcd_dma2d_clear(BLACK);
            line_count = 0; // 行计数器归零
        }

        // 计算本次打印的 Y 坐标
        current_y = START_Y + (line_count * LINE_SPACING);

        // 根据按键消息打印对应内容（注意 Y 坐标换成了 current_y）
        switch (msg.key_id)
        {
            case KEY0:
                if (msg.event == KEY_EVENT_PRESS) {
                    lcd_dma2d_show_eubf_str(0, current_y, (char*)"KEY0: 按顺序排队进来的", "字酷堂板桥体", FONT_SIZE, WHITE);
                } 
                else if (msg.event == KEY_EVENT_LONG_PRESS) {
                    lcd_dma2d_show_eubf_str(0, current_y, (char*)"KEY0: 长按触发！", "字酷堂板桥体", FONT_SIZE, YELLOW);
                }
                break;

            case KEY1:
                if (msg.event == KEY_EVENT_PRESS) {
                    lcd_dma2d_show_eubf_str(0, current_y, (char*)"KEY1: 诸行无常 是生灭法", "字酷堂板桥体", FONT_SIZE, WHITE);
                }
                break;

            case KEY2:
                if (msg.event == KEY_EVENT_PRESS) {
                    lcd_dma2d_show_eubf_str(0, current_y, (char*)"KEY2: 按顺序排队进来的", "字酷堂板桥体", FONT_SIZE, WHITE);
                }
                break;

            case KEY_UP:
                if (msg.event == KEY_EVENT_PRESS) {
                    // KEY_UP 是高优先级的插队消息，换红色
                    lcd_dma2d_show_eubf_str(0, current_y, (char*)"KEY_UP: 我插队啦！！！", "字酷堂板桥体", FONT_SIZE, RED); 
                }
                break;

            default:
                break;
        }

        // 将绘制内容推送到物理屏幕上
        lcd_dma2d_update_screen();
        
        // 行数加 1
        line_count++;

        
        osDelay(1000); 
    }
  }
  /* USER CODE END App_Task_Draw */
}

/* USER CODE BEGIN Header_App_Task_ScanKeys */
/**
* @brief Function implementing the Task_ScanKeys thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_App_Task_ScanKeys */
void App_Task_ScanKeys(void *argument)
{
  /* USER CODE BEGIN App_Task_ScanKeys */
  
  // 1. 实例化并初始化所有按键对象
  KeyDevice_t keys[KEY_NUM] = {
      {.id = KEY0,   .last_state = 0, .press_tick = 0, .is_long_pressed = 0},
      {.id = KEY1,   .last_state = 0, .press_tick = 0, .is_long_pressed = 0},
      {.id = KEY2,   .last_state = 0, .press_tick = 0, .is_long_pressed = 0},
      {.id = KEY_UP, .last_state = 0, .press_tick = 0, .is_long_pressed = 0}
  };

  KeyMessage_t msg; // 用于发送队列的局部变量

  /* Infinite loop */
  for(;;)
  {
    for(int i = 0; i < KEY_NUM; i++)
    {
      // 读取当前按键状态
      keys[i].current_state = myGetKeyPressStateByID(keys[i].id);

      // --- 状态机逻辑开始 ---
      if (keys[i].current_state == 1) 
      {
         // 状态：正在被按下
         if (keys[i].last_state == 0) 
         {
             // 动作：刚刚按下 (上升沿)
             keys[i].press_tick = 0;
             keys[i].is_long_pressed = 0; // 清除长按标志
         }
         else 
         {
             // 动作：一直按住不松 (持续高电平)
             keys[i].press_tick++;
             
             // 判断：达到长按时间阈值 且 尚未触发过长按
             if (keys[i].press_tick >= KEY_LONG_PRESS_TICK && !keys[i].is_long_pressed)
             {
                 keys[i].is_long_pressed = 1; // 锁定标志位，防止疯狂触发
                 
                 msg.key_id = keys[i].id;
                 msg.event  = KEY_EVENT_LONG_PRESS; // 产生长按事件

                 // 发送消息入队 (KEY_UP 享受 VIP 插队待遇)
                 if (msg.key_id == KEY_UP) {
                     xQueueSendToFront(Queue_KeysHandle, &msg, 0);
                 } else {
                     xQueueSend(Queue_KeysHandle, &msg, 0);
                 }
             }
         }
      }
      else 
      {
         // 状态：处于松开状态
         if (keys[i].last_state == 1) 
         {
             // 动作：刚刚松开手 (下降沿)
             
             // 如果松手前【没有】触发过长按，并且按下的时间大于消抖时间，则判定为“短按”
             if (!keys[i].is_long_pressed && keys[i].press_tick >= KEY_DEBOUNCE_TICK)
             {
                 msg.key_id = keys[i].id;
                 msg.event  = KEY_EVENT_PRESS; // 产生短按事件

                 if (msg.key_id == KEY_UP) {
                     xQueueSendToFront(Queue_KeysHandle, &msg, 0);
                 } else {
                     xQueueSend(Queue_KeysHandle, &msg, 0);
                 }
             }
         }
         // 动作：松开后清空计时器，为下一次按下做准备
         keys[i].press_tick = 0; 
      }

      // 更新历史状态
      keys[i].last_state = keys[i].current_state; 
    }
    LEDG_Toggle();
    
    osDelay(20); 
  }
  /* USER CODE END App_Task_ScanKeys */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

