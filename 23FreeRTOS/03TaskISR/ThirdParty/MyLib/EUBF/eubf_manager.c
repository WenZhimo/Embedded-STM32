/**
 ******************************************************************************
 * @file    eubf_manager.c
 * @brief   EUBF 字库管理器 (无状态架构 & 工业级硬件容错版)
 ******************************************************************************
 */

#include "eubf_manager.h"
#include <string.h>
#include <stdio.h>


/* 外部系统资源 */

extern uint32_t HAL_GetTick(void);



/* ========================================================================= *
 * 内部函数声明
 * ========================================================================= */

static uint32_t read_u32_le_safe(int8_t fd, uint32_t offset);
static uint16_t read_u16_le_safe(int8_t fd, uint32_t offset);
static const char* Translate_Font_Alias(const char *request_name);
static int8_t Find_Available_Slot(void);
static uint8_t UTF8_To_Unicode(const char *utf8_str, uint16_t *unicode);
static int8_t EUBF_Read_Glyph_From_Disk(EUBF_Slot *slot, uint16_t unicode, EUBF_GlyphCache *target_node);
static int8_t EUBF_Get_Or_Load_Slot(const char *font_name, uint8_t size);

/* ========================================================================= *
 * 内部资源
 * ========================================================================= */

static EUBF_Port_Config_t s_io_port;  // 保存用户传入的底层接口

static int8_t s_file_fds[EUBF_MAX_SLOTS]; // 存储底层返回的 fd (如 FastSlot 的槽位号)

static uint8_t s_meta_temp_buf[16] __attribute__((aligned(4)));

static uint32_t s_slot_last_used_tick[EUBF_MAX_SLOTS] = {0};

static int8_t s_last_matched_slot = -1;

/* ========================================================================= *
 * EUBF Header
 * ========================================================================= */

#pragma pack(push,1)
typedef struct {

    char     magic[4];
    uint16_t version;

    char     font_name[32];
    uint16_t font_size;

    uint16_t ascent;
    uint16_t descent;
    uint16_t line_height;
    uint16_t max_advance;

    uint8_t  bpp;
    uint8_t  padding1;

    uint16_t max_width;
    uint16_t max_height;

    uint32_t glyph_count;

    uint16_t missing_glyph;
    uint16_t padding2;

    uint32_t page_dir_offset;
    uint32_t page_table_offset;
    uint32_t glyph_offset_offset;
    uint32_t glyph_data_offset;

    char     source_ttf[64];
    uint32_t ttf_crc;

} EUBF_RawHeader;
#pragma pack(pop)

/* ========================================================================= *
 * 字体别名
 * ========================================================================= */

typedef struct {
    const char *alias;
    const char *real_dir;
} EUBF_AliasMap;

static const EUBF_AliasMap g_font_aliases[] = {

    {"华康金文体", "DFJinWenW3-GB"},
    {"白路彤彤手写体", "bailutongtongshouxieti"},
    {"字酷堂板桥体", "zktbqt"},
    {"峄山碑篆体", "ysbzt"},
    {"余繁新语","yufanxinyu"}

};

#define ALIAS_TABLE_SIZE (sizeof(g_font_aliases) / sizeof(g_font_aliases[0]))


/* ========================================================================= *
 * 初始化 (依赖注入)
 * ========================================================================= */
void EUBF_Init(EUBF_Port_Config_t *config)
{
    if (config != NULL) {
        s_io_port = *config; // 拷贝用户配置
    }

    for (int i = 0; i < EUBF_MAX_SLOTS; i++)
    {
        g_eubf_slots[i].is_occupied = 0;
        s_slot_last_used_tick[i] = 0;
        s_file_fds[i] = -1; 
    }
    s_last_matched_slot = -1;
}


/* ========================================================================= *
 * 安全读取辅助函数 (改为 Offset-based 偏移量读取)
 * ========================================================================= */
static uint32_t read_u32_le_safe(int8_t fd, uint32_t offset)
{
    if (s_io_port.ReadAt(fd, offset, s_meta_temp_buf, 4) == 0) { // 0表示成功
        return (uint32_t)(s_meta_temp_buf[0] | (s_meta_temp_buf[1] << 8) | 
                          (s_meta_temp_buf[2] << 16) | (s_meta_temp_buf[3] << 24));
    }
    return 0xFFFFFFFF;
}

static uint16_t read_u16_le_safe(int8_t fd, uint32_t offset)
{
    if (s_io_port.ReadAt(fd, offset, s_meta_temp_buf, 2) == 0) {
        return (uint16_t)(s_meta_temp_buf[0] | (s_meta_temp_buf[1] << 8));
    }
    return 0xFFFF;
}
/* ========================================================================= *
 * Slot 自动调度
 * ========================================================================= */

static int8_t EUBF_Get_Or_Load_Slot(const char *font_name, uint8_t size)
{
    if (font_name == NULL) return -1;

    const char *actual_dir = Translate_Font_Alias(font_name);

    /* 快速路径 */

    if (s_last_matched_slot >= 0 &&
        g_eubf_slots[s_last_matched_slot].is_occupied)
    {
        if (g_eubf_slots[s_last_matched_slot].font_size == size &&
            strcmp(g_eubf_slots[s_last_matched_slot].font_name, actual_dir) == 0)
        {
            s_slot_last_used_tick[s_last_matched_slot] = HAL_GetTick();
            return s_last_matched_slot;
        }
    }

    /* 查找已加载字体 */

    for (int8_t i = 0; i < EUBF_MAX_SLOTS; i++)
    {
        if (g_eubf_slots[i].is_occupied &&
            g_eubf_slots[i].font_size == size &&
            strcmp(g_eubf_slots[i].font_name, actual_dir) == 0)
        {
            s_slot_last_used_tick[i] = HAL_GetTick();
            s_last_matched_slot = i;
            return i;
        }
    }

    /* 查找可用 slot */
    int8_t slot_idx = Find_Available_Slot();
    EUBF_Slot *slot = &g_eubf_slots[slot_idx];

    if (slot->is_occupied && s_file_fds[slot_idx] >= 0)
    {
        s_io_port.Close(s_file_fds[slot_idx]); // 使用用户接口关闭旧文件
        s_file_fds[slot_idx] = -1;
        slot->is_occupied = 0;
    }

    // 拼接路径，使用动态注册的 RootPath
    static char path_buf[128];
    snprintf(path_buf, sizeof(path_buf), "%sasset/font/%s/%d.eubf", 
             s_io_port.RootPath, actual_dir, size);

    // 调用底层 Open 接口 (比如 USB_FastSlot_Open_UTF8)
    int8_t fd = s_io_port.Open(path_buf);
    if (fd < 0) return -1;
    
    s_file_fds[slot_idx] = fd; // 保存底层返回的句柄/槽位号

    // 读取 Header (偏移量 0)
    EUBF_RawHeader head;
    if (s_io_port.ReadAt(fd, 0, (uint8_t*)&head, sizeof(EUBF_RawHeader)) != 0 ||
        memcmp(head.magic, "EUBF", 4) != 0)
    {
        s_io_port.Close(fd);
        s_file_fds[slot_idx] = -1;
        return -1;
    }


    strcpy(slot->font_name, actual_dir);

    slot->font_size = size;
    slot->line_height = head.line_height;
    slot->bpp = head.bpp;
    slot->missing_glyph = head.missing_glyph;

    slot->page_dir_offset = head.page_dir_offset;
    slot->glyph_offset_offset = head.glyph_offset_offset;
    slot->glyph_data_offset = head.glyph_data_offset;

    for (uint16_t c = 0; c < EUBF_MAX_CACHE_NODES; c++)
    {
        slot->cache_pool[c].unicode = 0xFFFF;
        slot->cache_pool[c].last_used_tick = 0;
    }

    slot->is_occupied = 1;

    s_slot_last_used_tick[slot_idx] = HAL_GetTick();
    s_last_matched_slot = slot_idx;

    return slot_idx;
}

/* ========================================================================= *
 * 获取 Glyph
 * ========================================================================= */

const EUBF_GlyphCache*
EUBF_Get_Glyph(const char *font_name,
               uint8_t size,
               uint16_t unicode)
{
    int8_t slot_idx = EUBF_Get_Or_Load_Slot(font_name, size);

    if (slot_idx < 0)
        return NULL;

    EUBF_Slot *slot = &g_eubf_slots[slot_idx];

    int16_t target_index = -1;
    int16_t oldest_index = 0;

    uint32_t oldest_tick = 0xFFFFFFFF;
    uint32_t current_tick = HAL_GetTick();

    for (int i = 0; i < EUBF_MAX_CACHE_NODES; i++)
    {
        if (slot->cache_pool[i].unicode == unicode)
        {
            slot->cache_pool[i].last_used_tick = current_tick;
            return &slot->cache_pool[i];
        }

        if (slot->cache_pool[i].unicode == 0xFFFF)
        {
            target_index = i;
        }
        else if (target_index == -1 &&
                 slot->cache_pool[i].last_used_tick < oldest_tick)
        {
            oldest_tick = slot->cache_pool[i].last_used_tick;
            oldest_index = i;
        }
    }

    if (target_index == -1)
        target_index = oldest_index;

    EUBF_GlyphCache *node = &slot->cache_pool[target_index];

    if (EUBF_Read_Glyph_From_Disk(slot, unicode, node) == 0)
    {
        node->unicode = unicode;
        node->last_used_tick = current_tick;
        return node;
    }

    return NULL;
}

/* ========================================================================= *
 * 获取行高
 * ========================================================================= */

uint16_t EUBF_Get_Line_Height(const char *font_name, uint8_t size)
{
    int8_t slot_idx = EUBF_Get_Or_Load_Slot(font_name, size);

    if (slot_idx < 0)
        return 0;

    return g_eubf_slots[slot_idx].line_height;
}

/* ========================================================================= *
 * 计算字符串宽度
 * ========================================================================= */

uint16_t EUBF_Get_Text_Width(const char *font_name,
                             uint8_t size,
                             const char *str)
{
    if (!str)
        return 0;

    if (EUBF_Get_Or_Load_Slot(font_name, size) < 0)
        return 0;

    uint16_t total = 0;

    const char *p = str;

    while (*p)
    {
        uint16_t code;
        uint8_t len = UTF8_To_Unicode(p, &code);

        if (len == 0)
            break;

        const EUBF_GlyphCache *glyph =
            EUBF_Get_Glyph(font_name, size, code);

        if (glyph)
            total += glyph->advance;

        p += len;
    }

    return total;
}

/* ========================================================================= *
 * USB 断开
 * ========================================================================= */

void EUBF_Notify_Disconnected(void)
{
    for (int i = 0; i < EUBF_MAX_SLOTS; i++)
        g_eubf_slots[i].is_occupied = 0;

    s_last_matched_slot = -1;
}



/* ========================================================================= *
 * 从磁盘读取 glyph (全面使用 ReadAt 接口)
 * ========================================================================= */
static int8_t EUBF_Read_Glyph_From_Disk(EUBF_Slot *slot, uint16_t unicode, EUBF_GlyphCache *target_node)
{
    int8_t s_idx = (slot - g_eubf_slots);
    int8_t fd = s_file_fds[s_idx]; // 获取绑定的底层文件槽位号

    // 1. 读 Page 偏移
    uint32_t page_offset = read_u32_le_safe(fd, slot->page_dir_offset + ((unicode >> 8) * 4));
    uint16_t glyph_id = 0xFFFF;

    // 2. 读 Glyph ID
    if (page_offset != 0xFFFFFFFF) {
        glyph_id = read_u16_le_safe(fd, page_offset + ((unicode & 0xFF) * 2));
    }
    if (page_offset == 0xFFFFFFFF || glyph_id == 0xFFFF) {
        glyph_id = slot->missing_glyph;
    }

    // 3. 读数据偏移
    uint32_t rel_addr = read_u32_le_safe(fd, slot->glyph_offset_offset + (glyph_id * 4));

    // 4. 读 Glyph 头部信息 (8个字节)
    if (s_io_port.ReadAt(fd, slot->glyph_data_offset + rel_addr, s_meta_temp_buf, 8) != 0) {
        return -1;
    }

    target_node->box_w    = s_meta_temp_buf[0];
    target_node->box_h    = s_meta_temp_buf[1];
    target_node->x_offset = (int8_t)s_meta_temp_buf[2];
    target_node->y_offset = (int8_t)s_meta_temp_buf[3];
    target_node->advance  = (uint16_t)(s_meta_temp_buf[4] | (s_meta_temp_buf[5] << 8));

    if (target_node->box_h == 0 && target_node->advance == 0) return -1;

    // 5. 读点阵数据
    if (target_node->box_h > 0 && target_node->box_w > 0)
    {
        uint32_t b_size = ((target_node->box_w * slot->bpp + 7) / 8) * target_node->box_h;
        if (b_size > 0 && b_size <= EUBF_MAX_BITMAP_SIZE)
        {
            // 继续从上面读完8字节的偏移量往后读数据
            if (s_io_port.ReadAt(fd, slot->glyph_data_offset + rel_addr + 8, target_node->bitmap_data, b_size) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

/* ========================================================================= *
 * 字体别名转换
 * ========================================================================= */

static const char*
Translate_Font_Alias(const char *request_name)
{
    for (int i = 0; i < ALIAS_TABLE_SIZE; i++)
    {
        if (strcmp(request_name, g_font_aliases[i].alias) == 0)
            return g_font_aliases[i].real_dir;
    }

    return request_name;
}

/* ========================================================================= *
 * Slot LRU
 * ========================================================================= */

static int8_t Find_Available_Slot(void)
{
    int8_t oldest_slot = 0;
    uint32_t oldest_tick = 0xFFFFFFFF;

    for (int8_t i = 0; i < EUBF_MAX_SLOTS; i++)
    {
        if (g_eubf_slots[i].is_occupied == 0)
            return i;

        if (s_slot_last_used_tick[i] < oldest_tick)
        {
            oldest_tick = s_slot_last_used_tick[i];
            oldest_slot = i;
        }
    }

    return oldest_slot;
}

/* ========================================================================= *
 * UTF8 → Unicode
 * ========================================================================= */

static uint8_t UTF8_To_Unicode(const char *utf8_str,
                               uint16_t *unicode)
{
    uint8_t first = (uint8_t)utf8_str[0];

    if (first < 0x80)
    {
        *unicode = first;
        return 1;
    }
    else if ((first & 0xE0) == 0xC0)
    {
        *unicode =
            ((first & 0x1F) << 6) |
            (utf8_str[1] & 0x3F);

        return 2;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        *unicode =
            ((first & 0x0F) << 12) |
            ((utf8_str[1] & 0x3F) << 6) |
            (utf8_str[2] & 0x3F);

        return 3;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        /* 4字节UTF8 → 截断为16bit */
        *unicode =
            ((utf8_str[2] & 0x3F) << 6) |
            (utf8_str[3] & 0x3F);

        return 4;
    }

    return 0;
}
