#ifndef __MYUSART_H
#define __MYUSART_H

#include "usart.h"

#define MAX_CMD_LEN 256

extern uint8_t rx_buffer_1[MAX_CMD_LEN];
extern uint8_t rx_buffer_1_flag;

// 声明初始化函数
void MyUSART_Init(void);

#endif // __MYUSART_H