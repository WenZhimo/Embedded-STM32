/**
 ******************************************************************************
 * @file    eubf_manager.c
 * @brief   EUBF 字库管理器 (无状态架构 & 工业级硬件容错版)
 ******************************************************************************
 */

#include "../../ThirdParty/MyLib/myinclude.h"
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

uint8_t s_meta_temp_buf[16] __attribute__((aligned(4)));// 临时缓冲区，用于存储字体元数据的缓存

static uint32_t s_slot_last_used_tick[EUBF_MAX_SLOTS] = {0};// 每个槽位的最后使用时间戳，用于LRU替换

static int8_t s_last_matched_slot = -1;// 最后匹配的槽位索引，用于LRU替换

static uint32_t s_lru_logic_tick = 0; // 全局LRU逻辑时钟

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

    // {"华康金文体", "DFJinWenW3-GB"},
    // {"白路彤彤手写体", "bailutongtongshouxieti"},
    // {"字酷堂板桥体", "zktbqt"},
    // {"峄山碑篆体", "ysbzt"},
    // {"余繁新语","yufanxinyu"}

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
/**
 * @brief 从文件安全读取32位小端序数据
 * @param fd 文件描述符
 * @param offset 读取的起始偏移量
 * @return 读取的32位数据（小端序转换后）
 * @return 0xFFFFFFFF 读取失败
 * @note 该函数从指定偏移量读取4个字节，按照小端序转换为32位整数
 */
static uint32_t read_u32_le_safe(int8_t fd, uint32_t offset)
{
    // 从指定偏移量读取4个字节到临时缓冲区
    if (s_io_port.ReadAt(fd, offset, s_meta_temp_buf, 4) == 0) { // 0表示成功
        // 将4个字节按照小端序转换为32位整数
        // 小端序：低位字节在低地址，高位字节在高地址
        return (uint32_t)(s_meta_temp_buf[0] | (s_meta_temp_buf[1] << 8) | 
                          (s_meta_temp_buf[2] << 16) | (s_meta_temp_buf[3] << 24));
    }
    // 读取失败返回错误值
    return 0xFFFFFFFF;
}

/**
 * @brief 从文件安全读取16位小端序数据
 * @param fd 文件描述符
 * @param offset 读取的起始偏移量
 * @return 读取的16位数据（小端序转换后）
 * @return 0xFFFF 读取失败
 * @note 该函数从指定偏移量读取2个字节，按照小端序转换为16位整数
 */
static uint16_t read_u16_le_safe(int8_t fd, uint32_t offset)
{
    // 从指定偏移量读取2个字节到临时缓冲区
    if (s_io_port.ReadAt(fd, offset, s_meta_temp_buf, 2) == 0) {
        // 将2个字节按照小端序转换为16位整数
        // 小端序：低位字节在低地址，高位字节在高地址
        return (uint16_t)(s_meta_temp_buf[0] | (s_meta_temp_buf[1] << 8));
    }
    // 读取失败返回错误值
    return 0xFFFF;
}
/* ========================================================================= *
 * Slot 自动调度
 * ========================================================================= */

/**
 * @brief 获取或加载字体槽位
 * @param font_name 字体名称（可以是别名）
 * @param size 字体大小
 * @return >=0 成功返回槽位索引（0~EUBF_MAX_SLOTS-1）
 * @return -1 失败（参数无效、文件打开失败或文件头验证失败）
 * @note 该函数采用三级查找策略：
 *       1. 快速路径：检查上次匹配的槽位
 *       2. 遍历查找：检查所有已加载的字体槽位
 *       3. 加载新字体：查找可用槽位并加载字体文件
 */
static int8_t EUBF_Get_Or_Load_Slot(const char *font_name, uint8_t size)
{
    // 检查参数有效性
    if (font_name == NULL) return -1;

    // 将字体别名转换为实际目录名
    const char *actual_dir = Translate_Font_Alias(font_name);

    /* 快速路径：检查上次匹配的槽位 */

    if (s_last_matched_slot >= 0 &&
        g_eubf_slots[s_last_matched_slot].is_occupied)
    {
        // 检查字体大小和名称是否匹配
        if (g_eubf_slots[s_last_matched_slot].font_size == size &&
            strcmp(g_eubf_slots[s_last_matched_slot].font_name, actual_dir) == 0)
        {
            // 更新最后使用时间
            s_slot_last_used_tick[s_last_matched_slot] = HAL_GetTick();
            // 返回匹配的槽位
            return s_last_matched_slot;
        }
    }

    /* 查找已加载字体：遍历所有槽位 */

    for (int8_t i = 0; i < EUBF_MAX_SLOTS; i++)
    {
        // 检查槽位是否被占用，字体大小和名称是否匹配
        if (g_eubf_slots[i].is_occupied &&
            g_eubf_slots[i].font_size == size &&
            strcmp(g_eubf_slots[i].font_name, actual_dir) == 0)
        {
            // 更新最后使用时间
            s_slot_last_used_tick[i] = HAL_GetTick();
            // 更新最后匹配的槽位
            s_last_matched_slot = i;
            // 返回匹配的槽位
            return i;
        }
    }

    /* 查找可用槽位并加载新字体 */
    int8_t slot_idx = Find_Available_Slot();
    EUBF_Slot *slot = &g_eubf_slots[slot_idx];

    // 如果槽位已被占用且文件已打开，先关闭旧文件
    if (slot->is_occupied && s_file_fds[slot_idx] >= 0)
    {
        s_io_port.Close(s_file_fds[slot_idx]); // 使用用户接口关闭旧文件
        s_file_fds[slot_idx] = -1;
        slot->is_occupied = 0;
    }

    // 拼接字体文件路径，使用动态注册的 RootPath
    static char path_buf[128];
    snprintf(path_buf, sizeof(path_buf), "%sasset/font/%s/%d.eubf", 
             s_io_port.RootPath, actual_dir, size);

    // 调用底层 Open 接口打开字体文件 (比如 USB_FastSlot_Open_UTF8)
    int8_t fd = s_io_port.Open(path_buf);
    if (fd < 0) return -1;
    
    // 保存底层返回的句柄/槽位号
    s_file_fds[slot_idx] = fd;

    // 读取文件头 (偏移量 0)
    EUBF_RawHeader head;
    if (s_io_port.ReadAt(fd, 0, (uint8_t*)&head, sizeof(EUBF_RawHeader)) != 0 ||
        memcmp(head.magic, "EUBF", 4) != 0)
    {
        // 文件头验证失败，关闭文件
        s_io_port.Close(fd);
        s_file_fds[slot_idx] = -1;
        return -1;
    }

    // 初始化槽位信息
    strcpy(slot->font_name, actual_dir);

    // 设置字体基本属性
    slot->font_size = size;
    slot->line_height = head.line_height;
    slot->bpp = head.bpp;
    slot->missing_glyph = head.missing_glyph;

    // 设置文件偏移量信息
    slot->page_dir_offset = head.page_dir_offset;
    slot->glyph_offset_offset = head.glyph_offset_offset;
    slot->glyph_data_offset = head.glyph_data_offset;

    // 初始化缓存池
    for (uint16_t c = 0; c < EUBF_MAX_CACHE_NODES; c++)
    {
        slot->cache_pool[c].unicode = 0xFFFF;
        slot->cache_pool[c].last_used_tick = 0;
    }

    // 标记槽位为已占用
    slot->is_occupied = 1;

    // 更新最后使用时间和最后匹配的槽位
    s_slot_last_used_tick[slot_idx] = HAL_GetTick();
    s_last_matched_slot = slot_idx;

    // 返回槽位索引
    return slot_idx;
}

/* ========================================================================= *
 * 获取 Glyph
 * ========================================================================= */

/**
 * @brief 获取指定Unicode字符的字形数据
 * @param font_name 字体名称（可以是别名）
 * @param size 字体大小
 * @param unicode Unicode字符码点
 * @return 成功返回字形缓存节点的指针
 * @return NULL 失败（字体加载失败或磁盘读取失败）
 * @note 该函数采用两级缓存策略：
 *       1. 缓存命中：直接从缓存池返回字形数据
 *       2. 缓存未命中：从磁盘读取字形数据并缓存
 *       缓存替换策略：优先使用空闲节点，否则使用LRU（最近最少使用）策略
 */
const EUBF_GlyphCache*
EUBF_Get_Glyph(const char *font_name,
               uint8_t size,
               uint16_t unicode)
{
    // 获取或加载字体槽位
    int8_t slot_idx = EUBF_Get_Or_Load_Slot(font_name, size);

    // 检查槽位是否有效
    if (slot_idx < 0)
        return NULL;

    // 获取字体槽位指针
    EUBF_Slot *slot = &g_eubf_slots[slot_idx];

    // 初始化目标缓存节点索引和最久未使用节点索引
    int16_t target_index = -1;
    int16_t oldest_index = 0;

    // 初始化最久未使用时间戳和当前时间戳
    uint32_t oldest_tick = 0xFFFFFFFF;
    //uint32_t current_tick = HAL_GetTick();

    // 遍历缓存池，查找目标Unicode字符
    for (int i = 0; i < EUBF_MAX_CACHE_NODES; i++)
    {
        // 缓存命中
        if (slot->cache_pool[i].unicode == unicode)
        {
            slot->cache_pool[i].last_used_tick = ++s_lru_logic_tick; // 使用自增逻辑时钟
            return &slot->cache_pool[i];
        }

        if (slot->cache_pool[i].unicode == 0xFFFF)
        {
            target_index = i;
            break; // 强烈建议加上 break！找到空闲节点就立刻停止遍历，提升性能
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
        node->last_used_tick = ++s_lru_logic_tick; // 使用自增逻辑时钟
        return node;
    }

    // 磁盘读取失败，返回NULL
    return NULL;
}

/* ========================================================================= *
 * 获取行高
 * ========================================================================= */

/**
 * @brief 获取指定字体的行高
 * @param font_name 字体名称（可以是别名）
 * @param size 字体大小
 * @return 字体的行高（像素）
 * @return 0 失败（字体加载失败）
 * @note 该函数会自动加载字体到缓存槽位中，然后返回字体的行高信息
 */
uint16_t EUBF_Get_Line_Height(const char *font_name, uint8_t size)
{
    // 获取或加载字体槽位
    int8_t slot_idx = EUBF_Get_Or_Load_Slot(font_name, size);

    // 检查槽位是否有效
    if (slot_idx < 0)
        return 0;

    // 返回字体的行高
    return g_eubf_slots[slot_idx].line_height;
}

/* ========================================================================= *
 * 计算字符串宽度
 * ========================================================================= */

/**
 * @brief 计算指定文本的像素宽度
 * @param font_name 字体名称（可以是别名）
 * @param size 字体大小
 * @param str 要计算的文本字符串（UTF-8编码）
 * @return 文本的总宽度（像素）
 * @return 0 失败（字符串为空或字体加载失败）
 * @note 该函数遍历字符串中的每个字符，获取其字形数据并累加advance值
 *       advance值表示字符在水平方向上的占用宽度，包括字距调整
 */
uint16_t EUBF_Get_Text_Width(const char *font_name,
                             uint8_t size,
                             const char *str)
{
    // 检查字符串指针是否为空
    if (!str)
        return 0;

    // 获取或加载字体槽位
    if (EUBF_Get_Or_Load_Slot(font_name, size) < 0)
        return 0;

    // 初始化总宽度
    uint16_t total = 0;

    // 指向字符串起始位置
    const char *p = str;

    // 遍历字符串中的每个字符
    while (*p)
    {
        // 将UTF-8字符转换为Unicode码点
        uint16_t code;
        uint8_t len = UTF8_To_Unicode(p, &code);

        // 如果转换失败（len == 0），退出循环
        if (len == 0)
            break;

        // 获取当前字符的字形数据
        const EUBF_GlyphCache *glyph =
            EUBF_Get_Glyph(font_name, size, code);

        // 如果获取成功，累加字符的advance值
        if (glyph)
            total += glyph->advance;

        // 移动到下一个字符
        p += len;
    }

    // 返回文本的总宽度
    return total;
}

/* ========================================================================= *
 * USB 断开
 * ========================================================================= */

/**
 * @brief 通知字体管理器设备已断开连接
 * @note 该函数用于在存储设备断开连接时清空所有字体槽位状态
 *       避免使用已失效的文件句柄，防止系统崩溃
 */
void EUBF_Notify_Disconnected(void)
{
    // 遍历所有字体槽位，清空占用状态
    for (int i = 0; i < EUBF_MAX_SLOTS; i++)
        g_eubf_slots[i].is_occupied = 0;

    // 重置最后匹配的槽位索引
    s_last_matched_slot = -1;
}



/* ========================================================================= *
 * 从磁盘读取 glyph (全面使用 ReadAt 接口)
 * ========================================================================= */
/**
 * @brief 从磁盘读取指定Unicode字符的字形数据
 * @param slot 字体槽位指针
 * @param unicode Unicode字符码点
 * @param target_node 目标缓存节点指针，用于存储读取的字形数据
 * @return 0 读取成功
 * @return -1 读取失败
 * @note 该函数采用三级索引结构查找字形数据：
 *       1. Page索引：使用Unicode高8位查找Page偏移量
 *       2. Glyph索引：使用Unicode低8位查找Glyph ID
 *       3. 数据索引：使用Glyph ID查找字形数据偏移量
 *       如果字符不存在，使用missing_glyph作为替代
 */
static int8_t EUBF_Read_Glyph_From_Disk(EUBF_Slot *slot, uint16_t unicode, EUBF_GlyphCache *target_node)
{
    // 计算槽位索引（通过指针差值计算）
    int8_t s_idx = (slot - g_eubf_slots);
    // 获取绑定的底层文件槽位号
    int8_t fd = s_file_fds[s_idx];
    
    // 【关键优化】：使用局部数组，避免 RTOS 多任务重入导致数据相互覆盖！
    uint8_t buf = {0}; 

    // 1. 读Page偏移：使用 ReadAt 直接判断底层是否发生通信错误
    uint32_t page_offset;
    if (s_io_port.ReadAt(fd, slot->page_dir_offset + ((unicode >> 8) * 4), buf, 4) != 0) {
        return -1; // 发生了硬件读取错误，立即中止，坚决不缓存垃圾数据！
    }
    // 注意这里必须带上数组下标
    page_offset = (uint32_t)(buf | (buf << 8) | (buf << 16) | (buf << 24));

    uint16_t glyph_id = 0xFFFF;

    // 2. 读Glyph ID
    if (page_offset != 0xFFFFFFFF) {
        if (s_io_port.ReadAt(fd, page_offset + ((unicode & 0xFF) * 2), buf, 2) != 0) {
            return -1; // 发生了硬件读取错误，立即中止
        }
        glyph_id = (uint16_t)(buf | (buf << 8));
    }

    // 此时此刻，如果 page_offset == 0xFFFFFFFF，那说明字库是真的缺这个字，而不是底层读挂了
    if (page_offset == 0xFFFFFFFF || glyph_id == 0xFFFF) {
        glyph_id = slot->missing_glyph;
    }

    // 3. 读数据偏移：使用 ReadAt 直接判断底层是否发生通信错误
    uint32_t rel_addr;
    if (s_io_port.ReadAt(fd, slot->glyph_offset_offset + (glyph_id * 4), buf, 4) != 0) {
        return -1; // 发生了硬件读取错误，立即中止
    }
    rel_addr = (uint32_t)(buf | (buf << 8) | (buf << 16) | (buf << 24));

    // 4. 读Glyph头部信息（8个字节）
    if (s_io_port.ReadAt(fd, slot->glyph_data_offset + rel_addr, buf, 8) != 0) {
        return -1;
    }

    // 解析Glyph头部信息
    target_node->box_w    = buf;  // 字形宽度
    target_node->box_h    = buf;  // 字形高度
    target_node->x_offset = (int8_t)buf;  // X轴偏移量
    target_node->y_offset = (int8_t)buf;  // Y轴偏移量
    target_node->advance  = (uint16_t)(buf | (buf << 8));  // 字符前进宽度

    // 检查字形数据是否有效
    if (target_node->box_h == 0 && target_node->advance == 0) return -1;

    // 5. 读点阵数据
    if (target_node->box_h > 0 && target_node->box_w > 0)
    {
        // 计算点阵数据大小（考虑bpp位深度）
        uint32_t b_size = ((target_node->box_w * slot->bpp + 7) / 8) * target_node->box_h;
        // 检查数据大小是否在有效范围内
        if (b_size > 0 && b_size <= EUBF_MAX_BITMAP_SIZE)
        {
            // 继续从上面读完8字节的偏移量往后读数据
            if (s_io_port.ReadAt(fd, slot->glyph_data_offset + rel_addr + 8, target_node->bitmap_data, b_size) != 0) {
                return -1;
            }
        }
    }
    // 读取成功
    return 0;
}

/* ========================================================================= *
 * 字体别名转换
 * ========================================================================= */

/**
 * @brief 将字体别名转换为实际目录名
 * @param request_name 请求的字体名称（可以是别名）
 * @return 实际的字体目录名
 * @note 该函数在字体别名表中查找匹配的别名，如果找到则返回对应的实际目录名
 *       如果没有找到匹配的别名，则直接返回原始请求名称
 *       这种设计允许用户使用简短或易记的别名来访问字体，而不需要记住完整的目录名
 */
static const char*
Translate_Font_Alias(const char *request_name)
{
    // 遍历字体别名表
    for (int i = 0; i < ALIAS_TABLE_SIZE; i++)
    {
        // 检查请求的名称是否与别名表中的某个别名匹配
        if (strcmp(request_name, g_font_aliases[i].alias) == 0)
            // 找到匹配的别名，返回对应的实际目录名
            return g_font_aliases[i].real_dir;
    }

    // 没有找到匹配的别名，返回原始请求名称
    return request_name;
}

/* ========================================================================= *
 * Slot LRU
 * ========================================================================= */

/**
 * @brief 查找可用的字体槽位
 * @return 可用的槽位索引（0~EUBF_MAX_SLOTS-1）
 * @note 该函数采用两级查找策略：
 *       1. 优先查找空闲槽位（is_occupied == 0）
 *       2. 如果所有槽位都被占用，返回最久未使用的槽位（LRU策略）
 *       LRU策略确保频繁使用的字体保持在缓存中，提高缓存命中率
 */
static int8_t Find_Available_Slot(void)
{
    // 初始化最久未使用的槽位索引和时间戳
    int8_t oldest_slot = 0;
    uint32_t oldest_tick = 0xFFFFFFFF;

    // 遍历所有字体槽位
    for (int8_t i = 0; i < EUBF_MAX_SLOTS; i++)
    {
        // 优先查找空闲槽位
        if (g_eubf_slots[i].is_occupied == 0)
            // 找到空闲槽位，直接返回
            return i;

        // 记录最久未使用的槽位（LRU策略）
        if (s_slot_last_used_tick[i] < oldest_tick)
        {
            // 更新最久未使用的时间戳和槽位索引
            oldest_tick = s_slot_last_used_tick[i];
            oldest_slot = i;
        }
    }

    // 所有槽位都被占用，返回最久未使用的槽位
    return oldest_slot;
}

/* ========================================================================= *
 * UTF8 → Unicode
 * ========================================================================= */

/**
 * @brief 将UTF-8编码的字符转换为Unicode码点
 * @param utf8_str UTF-8编码的字符串
 * @param unicode 输出参数，用于存储转换后的Unicode码点
 * @return UTF-8字符的字节数（1-4）
 * @return 0 转换失败（无效的UTF-8编码）
 * @note 该函数支持1-4字节的UTF-8编码，但4字节编码会被截断为16位
 *       UTF-8编码规则：
 *       - 1字节：0xxxxxxx (0x00-0x7F) - ASCII字符
 *       - 2字节：110xxxxx 10xxxxxx (0xC0-0xDF)
 *       - 3字节：1110xxxx 10xxxxxx 10xxxxxx (0xE0-0xEF)
 *       - 4字节：11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (0xF0-0xF7)
 */
static uint8_t UTF8_To_Unicode(const char *utf8_str,
                               uint16_t *unicode)
{
    // 获取UTF-8字符串的第一个字节
    uint8_t first = (uint8_t)utf8_str[0];

    // 1字节UTF-8编码（0x00-0x7F）：ASCII字符，直接转换为Unicode
    if (first < 0x80)
    {
        *unicode = first;
        return 1;
    }
    // 2字节UTF-8编码（0xC0-0xDF）：解码为Unicode
    else if ((first & 0xE0) == 0xC0)
    {
        // 提取5位有效数据（第一个字节）和6位有效数据（第二个字节）
        *unicode =
            ((first & 0x1F) << 6) |
            (utf8_str[1] & 0x3F);

        return 2;
    }
    // 3字节UTF-8编码（0xE0-0xEF）：解码为Unicode
    else if ((first & 0xF0) == 0xE0)
    {
        // 提取4位有效数据（第一个字节）、6位有效数据（第二个字节）和6位有效数据（第三个字节）
        *unicode =
            ((first & 0x0F) << 12) |
            ((utf8_str[1] & 0x3F) << 6) |
            (utf8_str[2] & 0x3F);

        return 3;
    }
    // 4字节UTF-8编码（0xF0-0xF7）：截断为16位
    else if ((first & 0xF8) == 0xF0)
    {
        /* 4字节UTF8 → 截断为16bit */
        // 只保留后两个字节的有效数据，丢弃前两个字节（超出16位范围）
        *unicode =
            ((utf8_str[2] & 0x3F) << 6) |
            (utf8_str[3] & 0x3F);

        return 4;
    }

    // 无效的UTF-8编码，返回0
    return 0;
}