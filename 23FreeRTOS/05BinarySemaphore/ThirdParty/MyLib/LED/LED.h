#ifndef __LED_H__
#define __LED_H__
#include <stdint.h>


void LEDR(uint8_t state);
void LEDG(uint8_t state);
void LEDR_Toggle();
void LEDG_Toggle();

#endif /* __LED_H__ */