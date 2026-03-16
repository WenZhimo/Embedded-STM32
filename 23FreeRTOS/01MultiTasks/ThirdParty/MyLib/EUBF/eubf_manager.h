/**
 ******************************************************************************
 * @file    eubf_manager.h
 * @brief   EUBF 字库管理器 (无状态架构版)
 ******************************************************************************
 */
#ifndef __EUBF_MANAGER_H
#define __EUBF_MANAGER_H

#include <stdint.h>
#include "ff.h"            
#include "../../ThirdParty/MyLib/SDRAM/sdram.h" 

#define EUBF_MAX_SLOTS        10    
#define EUBF_MAX_CACHE_NODES  256   
#define EUBF_MAX_BITMAP_SIZE  2048  

#define EUBF_OK               0
#define EUBF_ERR_NO_FILE      -1    
#define EUBF_ERR_FORMAT       -2    

#pragma pack(push, 1)
/**
 * @brief 单个字符的 LRU 缓存节点 (存放在 SDRAM)
 */
typedef struct {
    uint16_t unicode;        
    uint32_t last_used_tick; 
    uint8_t  box_w;
    uint8_t  box_h;
    int8_t   x_offset;
    int8_t   y_offset;
    uint16_t advance;
    uint8_t  bitmap_data[EUBF_MAX_BITMAP_SIZE]; 
} EUBF_GlyphCache;

/**
 * @brief 单个字库槽位管理结构 (存放在 SDRAM)
 */
typedef struct {
    uint8_t  is_occupied;    
    char     font_name[32];  
    uint8_t  font_size;      
    uint16_t line_height;
    uint16_t bpp;
    uint16_t missing_glyph;
    uint32_t page_dir_offset;
    uint32_t page_table_offset;
    uint32_t glyph_offset_offset;
    uint32_t glyph_data_offset;
    EUBF_GlyphCache cache_pool[EUBF_MAX_CACHE_NODES]; 
} EUBF_Slot;
#pragma pack(pop)

/* 指向 SDRAM 的基地址指针 */
#define g_eubf_slots ((EUBF_Slot *)EUBF_CACHE_BASE_ADDR)

/* ========================================================================= *
 * 核心对外 API (无状态，上层直接索要资源)
 * ========================================================================= */

void EUBF_Init(void);

/**
 * @brief 直接获取字形信息 (底层自动完成加载、切换、LRU 淘汰与 U 盘读取)
 */
const EUBF_GlyphCache* EUBF_Get_Glyph(const char *font_name, uint8_t size, uint16_t unicode);

/**
 * @brief 获取指定字库的行高
 */
uint16_t EUBF_Get_Line_Height(const char *font_name, uint8_t size);

/**
 * @brief 获取字符串在指定字库下的总渲染宽度 (像素)
 */
uint16_t EUBF_Get_Text_Width(const char *font_name, uint8_t size, const char *str);

/**
 * @brief U 盘断开时的紧急清理与安全切断
 */
void EUBF_Notify_USB_Disconnected(void);

#endif /* __EUBF_MANAGER_H */