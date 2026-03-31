#ifndef __SDRAM_W9825G6KH_H
#define __SDRAM_W9825G6KH_H

#include "stm32f4xx_hal.h"

/* * W9825G6KH 容量为 32MB (256Mbit) 
 * 原理图片选连接至 FMC_SDNE0，映射到 STM32 的 SDRAM Bank 1
 */
#define SDRAM_BANK_ADDR     ((uint32_t)0xC0000000)
#define SDRAM_SIZE          ((uint32_t)0x2000000) // 32MB

/* FMC 命令超时时间 */
#define SDRAM_TIMEOUT       ((uint32_t)0xFFFF)

/* W9825G6KH 模式寄存器配置定义 */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)

#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)

#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)

#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)

#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

/* 外部引入 CubeMX 生成的 SDRAM 句柄 */
extern SDRAM_HandleTypeDef hsdram1;


/* ======================================  其它外设的配置统一在这里管理   =========================================*/
/* ------------------------------------------------------------------------------------------
 * [显存内存映射配置]
 * 依赖于 sdram.h 中的 SDRAM_BANK_ADDR (如 0xC0000000)
 * ------------------------------------------------------------------------------------------*/
/* 定义全屏显存首地址 (默认存放在 SDRAM 的起始位置) */
#define LCD_FRAME_BUFFER        SDRAM_BANK_ADDR
#define LCD_FRAME_BUFFER_SIZE    (320 * 480 * 2) // 320x480 分辨率，RGB565 格式每像素 2 字节，共 307200 字节
/* 定义字库/图片缓存首地址 (跳过全屏显存的大小) */
#define FONT_CACHE_BUFFER       (SDRAM_BANK_ADDR + LCD_FRAME_BUFFER_SIZE)
#define FONT_CACHE_BUFFER_SIZE  (320 * 480 * 2) 



/* * EUBF 字库 LRU 缓存区域 - 分配 6MB 
 * 紧接着显存区域之后。供我们定义的 10 个超级槽位使用。
 */
#define EUBF_CACHE_BASE_ADDR    (FONT_CACHE_BUFFER + FONT_CACHE_BUFFER_SIZE)
#define EUBF_CACHE_SIZE         (6 * 1024 * 1024)




/* 函数声明 */
void SDRAM_InitSequence(void);
void SDRAM_WriteBuffer8(uint32_t addr, uint8_t *data, uint32_t len);
void SDRAM_ReadBuffer8(uint32_t addr, uint8_t *data, uint32_t len);
void SDRAM_WriteBuffer16(uint32_t addr, uint16_t *data, uint32_t len);
void SDRAM_ReadBuffer16(uint32_t addr, uint16_t *data, uint32_t len);
void SDRAM_WriteBuffer32(uint32_t addr, uint32_t *data, uint32_t len);
void SDRAM_ReadBuffer32(uint32_t addr, uint32_t *data, uint32_t len);

uint8_t SDRAM_Test(void);

#endif /* __SDRAM_W9825G6KH_H */