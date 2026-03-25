/**
 ******************************************************************************
 * @file    eubf_manager.h
 * @brief   EUBF 字库管理器 (无状态架构版)
 * @author  文止墨
 * @date    2026-3-16
 * @note    https://github.com/S-R-Afraid/Embedded-Unicode-Bitmap-Font-Format
 ******************************************************************************
 */
#ifndef __EUBF_MANAGER_H
#define __EUBF_MANAGER_H

#include <stdint.h>
//#include "ff.h"            
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
 * 底层 IO 移植接口定义 (由用户实现并注册)
 * ========================================================================= */
typedef struct {
    /** * @brief 打开文件接口
     * @return 返回文件描述符(fd)或者槽位号(>=0)，失败返回 -1
     */
    int8_t  (*Open)(const char *path);
    
    /** * @brief 关闭文件接口
     */
    void    (*Close)(int8_t fd);
    
    /** * @brief 绝对偏移量读取接口 (对齐你的 FastSlot_Read 逻辑)
     * @return 0: 成功，其他: 失败
     */
    uint8_t (*ReadAt)(int8_t fd, uint32_t offset, uint8_t *buf, uint32_t len);
    
    /**
     * @brief 根目录路径 (例如 "0:/" 代表 SD卡，"1:/" 代表 U盘)
     */
    const char *RootPath; 
} EUBF_Port_Config_t;


/* ========================================================================= *
 * 核心对外 API
 * ========================================================================= */

/**
 * @brief 初始化管理器并注册底层 IO 接口
 * @param config 用户配置的底层 IO 函数指针结构体
 */
void EUBF_Init(EUBF_Port_Config_t *config);



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
void EUBF_Notify_Disconnected(void);

#endif /* __EUBF_MANAGER_H */