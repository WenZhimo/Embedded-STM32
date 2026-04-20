#include "../../ThirdParty/MyLib/myinclude.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "stream_buffer.h"



uint8_t rx_buffer_1[MAX_CMD_LEN];
uint8_t rx_buffer_1_flag = 0;

extern TaskHandle_t Task_CryptoHandle;
extern StreamBufferHandle_t xCryptoStreamBuffer; // 引入全局句柄

/* 串口与DMA接收初始化函数 */
void MyUSART_Init(void)
{
    // 开启串口空闲中断，接收数据到rx_buffer_1，使用DMA方式
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer_1, MAX_CMD_LEN);
    
    // 关闭半传输中断 (极其重要，防止接收到一半就触发中断)
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
}

/* GCC 专用的 printf 重定向函数 */
int _write(int file, char *data, int len) 
{
    // 确保只重定向标准输出 (stdout) 和标准错误 (stderr)
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO)) 
    {
        errno = EBADF;
        return -1;
    }
    
    // 使用 HAL_UART_Transmit 将数据发送出去
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 1000);
    return len;
}



/* 串口接收事件回调函数 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 1. 将接收到的数据一键送入 Stream Buffer
        if (xCryptoStreamBuffer != NULL) {
            xStreamBufferSendFromISR(xCryptoStreamBuffer, 
                                     rx_buffer_1, 
                                     Size, 
                                     &xHigherPriorityTaskWoken);
        }

        // 2. 重新启动 DMA 接收
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer_1, MAX_CMD_LEN);
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
        
        // 3. 立即触发上下文切换，让任务秒级响应
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}