#include "mykey.h"

#ifdef USE_RTOS
#include "cmsis_os.h"
#endif

// 按键对应的 GPIO 端口
static GPIO_TypeDef* keyGPIOPorts[] = 
{
    KEY0_GPIO_Port, 
    KEY1_GPIO_Port, 
    KEY2_GPIO_Port, 
    KEY_UP_GPIO_Port
};

// 按键对应的 GPIO 引脚
static uint16_t keyGPIOPins[] = 
{
    KEY0_Pin, 
    KEY1_Pin, 
    KEY2_Pin, 
    KEY_UP_Pin
};

// 按键激活电平
static KeyActiveLevel_t keyActiveLevels[] = 
{
    KEY_ACTIVE_LOW,
    KEY_ACTIVE_LOW,
    KEY_ACTIVE_LOW,
    KEY_ACTIVE_HIGH
};




uint8_t myGetKeyPressStateByID(KeyID_t keyid)
{
    if (keyid >= KEY_NUM) return 0; // 无效的按键 ID，返回未按下状态
    //软件消抖逻辑
    uint8_t key_state = 0;
    
    key_state = HAL_GPIO_ReadPin(keyGPIOPorts[keyid], keyGPIOPins[keyid]) == keyActiveLevels[keyid];
    #ifdef USE_RTOS
    osDelay(20); // 简单的软件消抖，延时20ms
    #else
    HAL_Delay(20); // 简单的软件消抖，延时20ms
    #endif

    if(key_state == (HAL_GPIO_ReadPin(keyGPIOPorts[keyid], keyGPIOPins[keyid]) == keyActiveLevels[keyid])){
        return key_state;
    } else {
        return 0; // 状态不稳定，认为没有按下
    }
}


