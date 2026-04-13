#ifndef __MYKEY_H__
#define __MYKEY_H__

#include "main.h"
#include <stdint.h>

#ifndef  KEY0_Pin
    #define KEY0_Pin GPIO_PIN_3
#endif

#ifndef  KEY0_GPIO_Port
    #define KEY0_GPIO_Port GPIOH
#endif

#ifndef  KEY1_Pin
    #define KEY1_Pin GPIO_PIN_2 
#endif

#ifndef  KEY1_GPIO_Port
    #define KEY1_GPIO_Port GPIOH
#endif

#ifndef  KEY2_Pin
    #define KEY2_Pin GPIO_PIN_13
#endif

#ifndef  KEY2_GPIO_Port
    #define KEY2_GPIO_Port GPIOC
#endif


#ifndef  KEY_UP_Pin
    #define KEY_UP_Pin GPIO_PIN_0
#endif

#ifndef  KEY_UP_GPIO_Port
    #define KEY_UP_GPIO_Port GPIOA
#endif

#define KEY_NUM 4
#define KEY_DEBOUNCE_TICK    1   // 20ms * 1 = 20ms (消抖阈值)
#define KEY_LONG_PRESS_TICK  50  // 20ms * 50 = 1000ms (长按1秒触发)

typedef enum {
  KEY0 = 0,
  KEY1,
  KEY2,
  KEY_UP,
  KEY_NONE
}KeyID_t;


// 按键激活状态类型
typedef enum {
    KEY_ACTIVE_LOW = 0, // 按下时为0
    KEY_ACTIVE_HIGH = 1 // 按下时为1
}KeyActiveLevel_t;

// 按键事件类型
typedef enum{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,
    KEY_EVENT_RELEASE,
    KEY_EVENT_LONG_PRESS,
    KEY_EVENT_DOUBLE_PRESS
} KeyEvent_t;


// 按键对象结构体
typedef struct {
    KeyID_t  id;              // 按键编号 (原 KeyName)
    uint8_t  current_state;   // 当前物理状态
    uint8_t  last_state;      // 上次物理状态
    uint16_t press_tick;      // 持续按下时间计数器
    uint8_t  is_long_pressed; // 标记标志：是否已经触发过长按
} KeyDevice_t;

uint8_t myGetKeyPressStateByID(KeyID_t keyid);





#endif
