/**
 ****************************************************************************************************
 * @file        lcd_dma2d.h
 * @brief       LCD DMA2D 图形加速与双缓冲管理模块
 * @note        依赖: lcd.h (底层屏幕接口), sdram.h (外部显存), dma2d (硬件加速)
 ****************************************************************************************************
 */

#ifndef __LCD_DMA2D_H
#define __LCD_DMA2D_H

#include "main.h"
#include "lcd.h"
#include "../../ThirdParty/MyLib/SDRAM/sdram.h"
#include "../../ThirdParty/MyLib/EUBF/eubf_manager.h"

/* ------------------------------------------------------------------------------------------
 * [显存内存映射配置]
 * 依赖于 sdram.h 中的 SDRAM_BANK_ADDR (如 0xC0000000)
 * ------------------------------------------------------------------------------------------*/
//具体配置请参考 sdram.h 中的注释

/* 外部引入的 HAL 库句柄 */
extern DMA2D_HandleTypeDef hdma2d;
/* 如果使用普通 DMA 刷屏，取消下面这行的注释并修改为你 CubeMX 里配置的句柄名 */
// extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

/* ------------------------------------------------------------------------------------------
 * [API 函数声明]
 * ------------------------------------------------------------------------------------------*/

/* 1. 基础缓存操作 */

 /* 模块初始化与清空缓存 */
void lcd_dma2d_init(void);
/* 将 SDRAM 缓存全速推送到 MCU 屏幕 */
void lcd_dma2d_update_screen(void); 
/* 在缓存中画单个像素点 */                                    
void lcd_buffer_draw_point(uint16_t x, uint16_t y, uint16_t color);
/* 在缓存中画单个像素点 (带透明度) */
void lcd_buffer_draw_pixel_alpha(uint16_t x, uint16_t y, uint16_t color, uint8_t alpha);
/* 在缓存中画一个带透明度的空心圆 (软件混合)*/
void lcd_buffer_draw_circle_alpha(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint8_t alpha);
/*使用 DMA2D 硬件加速画一个填充的半透明矩形*/
void lcd_buffer_fill_alpha(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color, uint8_t alpha);


/* 2. DMA2D 硬件加速 2D 绘图 (替代传统 for 循环) */
void lcd_dma2d_clear(uint16_t color);                                   /* 使用 DMA2D 极速清屏 */
void lcd_dma2d_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color); /* 使用 DMA2D 填充矩形 */

/* 3. 字体与图像高级混合 (Alpha Blending) */
void lcd_dma2d_draw_alpha_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *bitmap, uint32_t font_color);



/* 4. EUBF 字体渲染 */
/**
 * @brief   在显存缓存中显示 EUBF 字符串 (支持多语言、比例字体、抗锯齿)
 * @param   x, y      : 起始坐标
 * @param   str       : UTF-8 字符串
 * @param   font_slot : EUBF_Load_Font 返回的句柄
 * @param   color     : 字体颜色 (RGB565)
 */
void lcd_dma2d_show_eubf_str(uint16_t x, uint16_t y, const char *str, const char *font_name, uint8_t size, uint16_t color);
#endif /* __LCD_DMA2D_H */