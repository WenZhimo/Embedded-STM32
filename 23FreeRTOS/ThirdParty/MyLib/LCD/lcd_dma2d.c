/**
 ****************************************************************************************************
 * @file        lcd_dma2d.c
 * @brief       LCD DMA2D 图形加速与双缓冲管理模块实现
 ****************************************************************************************************
 */

#include "../../ThirdParty/MyLib/myinclude.h"

/* 定义一个指向画布的全局指针，方便在普通函数中操作像素 */
uint16_t *g_frame_buffer = (uint16_t *)LCD_FRAME_BUFFER;

/* ================= 引入与全局变量声明 ================= */
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

volatile uint8_t g_lcd_flush_done = 1;  /* 整体刷屏完成标志 */

/* 接力搬运专用的静态状态变量 */
static uint32_t dma_flush_total = 0;    /* 本次需要刷新的总像素数 */
static uint32_t dma_flush_sent = 0;     /* 已经发送的像素数 */
#define FLUSH_CHUNK_SIZE 60000          /* 每次搬运的最大块 */

/* 提前声明我们的自定义回调函数 */
void LCD_DMA_TransferComplete(DMA_HandleTypeDef *_hdma);


/**
 * @brief       初始化 DMA2D 图形模块
 * @param       无
 * @retval      无
 */
void lcd_dma2d_init(void)
{
    /* 确保 DMA2D 时钟已开启 (CubeMX 生成的 HAL_DMA2D_Init 通常会处理，这里做个双保险) */
    __HAL_RCC_DMA2D_CLK_ENABLE();
    
    /* 默认用白色清空显存缓存 */
    lcd_dma2d_clear(WHITE);
}

/**
 * @brief       在缓存中画单个像素点 (CPU 操作)
 * @param       x, y  : 坐标
 * @param       color : 颜色 (RGB565)
 */
void lcd_buffer_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= lcddev.width || y >= lcddev.height) 
    {
        return; /* 超出屏幕范围，丢弃 */
    }
    g_frame_buffer[y * lcddev.width + x] = color;
}

/**
 * @brief       使用 DMA2D 极速清空整个显存缓存
 * @param       color: 要清屏的颜色 (RGB565)
 */
void lcd_dma2d_clear(uint16_t color)
{
    /* 调用矩形填充接口，范围是整个屏幕 */
    lcd_dma2d_fill(0, 0, lcddev.width - 1, lcddev.height - 1, color);
}

/**
 * @brief       使用 DMA2D 硬件填充矩形区域 (Register-to-Memory 模式)
 * @note        速度极快，CPU 0% 占用
 * @param       sx,sy: 起点坐标
 * @param       ex,ey: 终点坐标
 * @param       color: 填充颜色 (RGB565)
 */
void lcd_dma2d_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint32_t width = ex - sx + 1;
    uint32_t height = ey - sy + 1;
    uint32_t offline = lcddev.width - width; // 行偏移
    
    /* 计算起始绝对地址 */
    uint32_t target_addr = (uint32_t)LCD_FRAME_BUFFER + (sy * lcddev.width + sx) * 2;

    /* 配置 DMA2D 寄存器 */
    DMA2D->CR = 0x00030000UL;                    /* Mode: Register-to-memory (R2M) */
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;         /* 输出格式: RGB565 */
    DMA2D->OMAR = target_addr;                   /* 目标内存地址 */
    DMA2D->OCOLR = color;                        /* 要填充的颜色 */
    DMA2D->OOR = offline;                        /* 输出行偏移 */
    DMA2D->NLR = (width << 16) | height;         /* 区域宽和高 */

    /* 启动传输并等待完成 */
    DMA2D->CR |= DMA2D_CR_START;
    while ((DMA2D->ISR & DMA2D_FLAG_TC) == 0);   /* 等待传输完成 */
    DMA2D->IFCR |= DMA2D_FLAG_TC;                /* 清除完成标志位 */
}

/**
 * @brief       渲染带 Alpha 通道的位图 (如 EUBF 字体)，并与当前缓存背景完美混合
 * @note        使用 Memory-to-Memory with Blending (M2M_BLEND) 模式
 * @param       x, y       : 屏幕坐标
 * @param       width      : 图像宽 (对应 EUBF meta.box_width)
 * @param       height     : 图像高 (对应 EUBF meta.box_height)
 * @param       bitmap     : A8 格式的点阵数据 (只包含透明度)
 * @param       font_color : 需要赋予的颜色 (RGB888格式，如 0xFF0000 为红)
 */
void lcd_dma2d_draw_alpha_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *bitmap, uint32_t font_color)
{
    /* 计算目标位置在 SDRAM 显存中的绝对地址 */
    uint32_t target_addr = (uint32_t)LCD_FRAME_BUFFER + (y * lcddev.width + x) * 2;
    uint32_t offset = lcddev.width - width;
    
    /* 1. 设置 DMA2D 模式为 M2M_BLEND */
    DMA2D->CR = 0x00020000UL; 

    /* 2. 配置前景层 (Layer 0) - 指向字库点阵 */
    DMA2D->FGPFCCR = DMA2D_INPUT_A8;             /* 前景格式: A8 (根据 EUBF 实际 bpp 调整) */
    DMA2D->FGMAR = (uint32_t)bitmap;             /* 前景数据源地址 */
    DMA2D->FGOR = 0;                             /* 前景行偏移 (字模数据通常是连续的，无偏移) */
    DMA2D->FGCOLR = font_color;                  /* 设定字体的颜色 */

    /* 3. 配置背景层 (Layer 1) - 指向 SDRAM 缓存 */
    DMA2D->BGPFCCR = DMA2D_INPUT_RGB565;         /* 背景格式: RGB565 */
    DMA2D->BGMAR = target_addr;                  /* 背景源地址 */
    DMA2D->BGOR = offset;                        /* 背景行偏移 */

    /* 4. 配置输出层 - 写回 SDRAM 缓存 */
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;         /* 输出格式: RGB565 */
    DMA2D->OMAR = target_addr;                   /* 输出目标地址 */
    DMA2D->OOR = offset;                         /* 输出行偏移 */
    
    /* 5. 配置区域大小 */
    DMA2D->NLR = (width << 16) | height;

    /* 6. 启动混合并等待完成 */
    DMA2D->CR |= DMA2D_CR_START;
    while ((DMA2D->ISR & DMA2D_FLAG_TC) == 0);   
    DMA2D->IFCR |= DMA2D_FLAG_TC;                
}



/**
 * @brief       使用DMA2分块接力自动刷屏 
 * @param       无
 */
void lcd_dma2d_update_screen(void)
{
    if (g_lcd_flush_done == 0) return; /* 防止上一帧没刷完就强行刷新 */

    /* 1. 设置 LCD 窗口，准备接收数据 */
    lcd_set_window(0, 0, lcddev.width, lcddev.height);
    lcd_write_ram_prepare();

    /* 2. 初始化接力参数 */
    dma_flush_total = lcddev.width * lcddev.height; // 153600
    dma_flush_sent = 0;
    g_lcd_flush_done = 0;

    /* 【核心修复！】手动绑定我们的接力赛回调函数 */
    hdma_memtomem_dma2_stream0.XferCpltCallback = LCD_DMA_TransferComplete;

    /* 3. 计算第一棒要跑多远 */
    uint32_t next_transfer = (dma_flush_total > FLUSH_CHUNK_SIZE) ? FLUSH_CHUNK_SIZE : dma_flush_total;

    /* 4. 启动第一棒！ */
    HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, 
                     (uint32_t)&g_frame_buffer[0], 
                     (uint32_t)&LCD->LCD_RAM, 
                     next_transfer);
}


/**
 * @brief       在 SDRAM 缓存中画一个带透明度的像素点 (软件混合)
 * @param       x, y   : 坐标
 * @param       color  : 前景颜色 (RGB565 格式)
 * @param       alpha  : 透明度 (0~255，0为全透明，255为全不透明)
 */
void lcd_buffer_draw_pixel_alpha(uint16_t x, uint16_t y, uint16_t color, uint8_t alpha)
{
    if (x >= lcddev.width || y >= lcddev.height) return; // 超出屏幕范围

    // 1. 获取背景颜色 (从 SDRAM 中读取当前像素)
    uint16_t bg_color = g_frame_buffer[y * lcddev.width + x];

    // 2. 如果全不透明，直接覆盖，不需要计算
    if (alpha == 255)
    {
        g_frame_buffer[y * lcddev.width + x] = color;
        return;
    }
    // 3. 如果全透明，直接返回，不画
    if (alpha == 0) return;

    // 4. 将前景和背景颜色从 RGB565 解包为 RGB888 通道
    // RGB565: RRRRR GGGGGG BBBBB
    uint8_t fg_r = (color >> 11) & 0x1F; // 5位
    uint8_t fg_g = (color >> 5) & 0x3F;  // 6位
    uint8_t fg_b = color & 0x1F;         // 5位

    uint8_t bg_r = (bg_color >> 11) & 0x1F;
    uint8_t bg_g = (bg_color >> 5) & 0x3F;
    uint8_t bg_b = bg_color & 0x1F;

    // 5. 应用 Alpha 混合公式：C_res = (C_fg * alpha + C_bg * (255 - alpha)) / 255
    // 为了提高速度，通常使用定点数运算，这里使用简单的整数运算
    uint8_t res_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
    uint8_t res_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
    uint8_t res_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

    // 6. 将结果打包回 RGB565 格式并写入显存
    uint16_t result_color = (res_r << 11) | (res_g << 5) | res_b;
    g_frame_buffer[y * lcddev.width + x] = result_color;
}

/**
 * @brief       在 SDRAM 缓存中画一个带透明度的空心圆 (软件混合)
 * @param       x0, y0 : 圆心坐标
 * @param       r      : 半径
 * @param       color  : 圆颜色 (RGB565)
 * @param       alpha  : 透明度 (0~255)
 */
void lcd_buffer_draw_circle_alpha(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint8_t alpha)
{
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 3 - 2 * r; // 决策变量

    // 利用对称性画八个分量
    while (x <= y)
    {
        lcd_buffer_draw_pixel_alpha(x0 + x, y0 + y, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 - x, y0 + y, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 + y, y0 + x, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 - y, y0 + x, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 + x, y0 - y, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 - x, y0 - y, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 + y, y0 - x, color, alpha);
        lcd_buffer_draw_pixel_alpha(x0 - y, y0 - x, color, alpha);

        if (d < 0)
        {
            d += 4 * x + 6;
        }
        else
        {
            d += 10 + 4 * (x - y);
            y--;
        }
        x++;
    }
}


/**
 * @brief       在 SDRAM 缓存中填充半透明矩形 (CPU 软件混合)
 * @param       sx, sy : 起始坐标
 * @param       ex, ey : 结束坐标
 * @param       color  : 填充的前景颜色 (RGB565)
 * @param       alpha  : 透明度 (0~255)
 */
void lcd_buffer_fill_alpha(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color, uint8_t alpha)
{
    if (alpha == 0) return; // 全透明不画

    // 1. 提前解包前景颜色，避免在循环中重复计算
    uint8_t fg_r = (color >> 11) & 0x1F;
    uint8_t fg_g = (color >> 5) & 0x3F;
    uint8_t fg_b = color & 0x1F;
    uint16_t inv_alpha = 255 - alpha;

    // 2. 遍历矩形区域
    for (uint16_t y = sy; y <= ey; y++)
    {
        for (uint16_t x = sx; x <= ex; x++)
        {
            // 如果超出屏幕，跳过
            if (x >= lcddev.width || y >= lcddev.height) continue;

            // 读取背景色
            uint16_t bg_color = g_frame_buffer[y * lcddev.width + x];

            // 解包背景色
            uint8_t bg_r = (bg_color >> 11) & 0x1F;
            uint8_t bg_g = (bg_color >> 5) & 0x3F;
            uint8_t bg_b = bg_color & 0x1F;

            // Alpha 混合计算
            uint8_t res_r = (fg_r * alpha + bg_r * inv_alpha) / 255;
            uint8_t res_g = (fg_g * alpha + bg_g * inv_alpha) / 255;
            uint8_t res_b = (fg_b * alpha + bg_b * inv_alpha) / 255;

            // 打包写回 SDRAM
            g_frame_buffer[y * lcddev.width + x] = (res_r << 11) | (res_g << 5) | res_b;
        }
    }
}

/**
 * @brief       我们自定义的 DMA 传输完成回调 (接力赛交接棒)
 * @param       _hdma: DMA 句柄
 */
void LCD_DMA_TransferComplete(DMA_HandleTypeDef *_hdma)
{
    if (_hdma == &hdma_memtomem_dma2_stream0)
    {
        /* 1. 刚刚这一棒跑了多远？累加进去 */
        uint32_t last_transfer = (dma_flush_total - dma_flush_sent > FLUSH_CHUNK_SIZE) ? FLUSH_CHUNK_SIZE : (dma_flush_total - dma_flush_sent);
        dma_flush_sent += last_transfer;

        /* 2. 还有剩下没跑完的像素吗？ */
        uint32_t remain = dma_flush_total - dma_flush_sent;
        
        if (remain > 0)
        {
            /* 还有剩余，继续跑下一棒！ */
            uint32_t next_transfer = (remain > FLUSH_CHUNK_SIZE) ? FLUSH_CHUNK_SIZE : remain;
            
            HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, 
                             (uint32_t)&g_frame_buffer[dma_flush_sent], // 源地址偏移
                             (uint32_t)&LCD->LCD_RAM,                   // 目标依然是固定寄存器
                             next_transfer);
        }
        else
        {
            /* 全部跑完了！升起终点旗帜 */
            g_lcd_flush_done = 1;
        }
    }
}



/**
 * @brief   内部 UTF-8 转 Unicode 辅助 (支持 1-3 字节)
 */
static uint8_t utf8_decode(const char *p, uint16_t *unicode) {
    uint8_t c = (uint8_t)*p;
    if (c < 0x80) { *unicode = c; return 1; }
    if ((c & 0xE0) == 0xC0) { *unicode = ((c & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if ((c & 0xF0) == 0xE0) { *unicode = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
    return 1;
}


/**
 * @brief   在显存缓存中显示 EUBF 字符串 (支持多语言、比例字体、抗锯齿)
 * @param   x, y      : 起始坐标
 * @param   str       : UTF-8 字符串
 * @param   font_slot : EUBF_Load_Font 返回的句柄
 * @param   color     : 字体颜色 (RGB565)
 */
void lcd_dma2d_show_eubf_str(uint16_t x, uint16_t y, const char *str, const char *font_name, uint8_t size, uint16_t color)
{
    uint16_t cur_x = x;
    uint16_t line_h = EUBF_Get_Line_Height(font_name, size); // 无状态获取行高
    
    while (*str) {
        if (*str == '\n') {
            cur_x = x; y += line_h; str++; continue;
        }

        uint16_t code=0;
        str += utf8_decode(str, &code); // 请确保您的工程里有这函数或使用内部的 UTF8_To_Unicode

        /* 底层自动分配槽位并返回所需字形，纳秒级命中 */
        const EUBF_GlyphCache *glyph = EUBF_Get_Glyph(font_name, size, code);
        
        if (glyph) {
            /* 1. 像素绘制（自动跳过空格等无像素字符） */
            if (glyph->box_w > 0 && glyph->box_h > 0) {
                uint16_t dx = cur_x + glyph->x_offset;
                uint16_t dy = y + glyph->y_offset;
                
                uint32_t b_idx = 0;
                for (uint16_t i = 0; i < glyph->box_h; i++) {
                    for (uint16_t j = 0; j < glyph->box_w; j++) {
                        uint8_t alpha4 = (j % 2 == 0) ? (glyph->bitmap_data[b_idx] >> 4) : (glyph->bitmap_data[b_idx] & 0x0F);
                        if (j % 2 != 0) b_idx++;
                        if (alpha4 > 0) {
                            lcd_buffer_draw_pixel_alpha(dx + j, dy + i, color, alpha4 * 17);
                        }
                    }
                    if (glyph->box_w % 2 != 0) b_idx++;
                }
            }
            
            /* 2. 更新坐标（即使是空格也会正常加上 advance 步进宽度） */
            cur_x += glyph->advance;
        }
    }
}