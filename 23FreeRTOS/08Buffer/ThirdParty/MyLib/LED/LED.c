#include "main.h"
#include "../../ThirdParty/MyLib/myinclude.h"
/**
* @brief 设置LEDR的状态
* @param state: LED的状态
* @retval None
*/
void LEDR(uint8_t state) {
   HAL_GPIO_WritePin(LEDR_GPIO_Port, LEDR_Pin, (GPIO_PinState)!state);
}

/**
* @brief 设置LEDG的状态
* @param state: LED的状态
* @retval None
*/
void LEDG(uint8_t state) {
    HAL_GPIO_WritePin(LEDG_GPIO_Port, LEDG_Pin, (GPIO_PinState)!state);
}
/**
* @brief 切换LEDR的状态
* @param None
* @retval None
*/
void LEDR_Toggle() {
    HAL_GPIO_TogglePin(LEDR_GPIO_Port, LEDR_Pin);
}
/**
* @brief 切换LEDG的状态
* @param None
* @retval None
*/
void LEDG_Toggle() {
    HAL_GPIO_TogglePin(LEDG_GPIO_Port, LEDG_Pin);
}
