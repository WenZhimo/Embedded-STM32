#include "../../ThirdParty/MyLib/myinclude.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

uint8_t rx_buffer_1[MAX_CMD_LEN];
uint8_t rx_buffer_1_flag = 0;

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
        // 此时一包数据接收完成
        // Size 变量就是本次实际接收到的字节数！非常方便！
        
        // 1. 数据处理
        // 示例：将接收到的指令丢给某个处理函数
        // ParseMotorCommand(rx_buffer_1, Size); 
        
        // HAL_UART_Transmit(&huart1, rx_buffer_1, Size, 100);
        rx_buffer_1_flag = Size; // 设置标志位，通知任务有新数据了

        // 2. 重新启动接收，准备接收下一帧变长指令
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer_1, MAX_CMD_LEN);
        
        // 紧接着关闭 DMA 的半传输中断 
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}