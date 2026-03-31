/**
  ******************************************************************************
  * @file    sd_app.c
  * @brief   SD/TF卡应用层底层驱动实现 (带常开槽位管理，适配无CD引脚硬件)
  ******************************************************************************
  */
#include "../../ThirdParty/MyLib/myinclude.h"
#include <string.h>

/* FatFs 生成的外部变量，通常在 fatfs.c 中定义 */
extern char SDPath[4]; /* SD logical drive path (例如 "0:/") */

/* 核心文件系统对象 */
FATFS SDFatFs;            
FIL NormalFile;                

/* --- 常开槽位管理池 --- */
FIL FastSlots[MAX_SD_FAST_SLOTS];               // 槽位文件对象数组
uint8_t slot_status[MAX_SD_FAST_SLOTS] = {0};   // 槽位状态：0为空闲，1为占用

uint8_t is_sd_mounted = 0;    

uint8_t SD_Is_Mounted(void) {
    return is_sd_mounted;
}

/* =======================================================================
 * 后台挂载管理
 * ======================================================================= */
void SD_Mount_StateMachine(void)
{
    /* * 由于硬件没有 Card Detect (CD) 引脚，我们采用“挂载即常驻”的策略。
     * 只要 is_sd_mounted 为 0，就不断尝试挂载。一旦挂载成功，就假设卡一直在线。
     */
    if (is_sd_mounted == 0) 
    {
        // 第三个参数为 1，表示立即尝试挂载，这样如果卡没插或者初始化失败，会立刻返回错误
        if (f_mount(&SDFatFs, (TCHAR const*)SDPath, 1) == FR_OK) {
            is_sd_mounted = 1;
            //printf("SD Card Mounted Successfully!\r\n");
            
        }
    }
}

#if SD_USE_RTOS == 1
void SD_App_Thread(void) {
    for(;;) {
        SD_Mount_StateMachine();
        osDelay(100); 
    }
}
#else
void SD_App_Process(void) {
    SD_Mount_StateMachine();
}
#endif

/**
 * @brief  在系统初始化时等待 SD 卡挂载，带 5 秒超时
 * @retval 0: 挂载成功; 1: 挂载失败（未插卡或初始化失败）
 */
uint8_t SD_Init_Wait_Mount(void)
{
    uint32_t TIMEOUT_COUNT = 50;    // 设置超时次数为 50 

    // 如果由于某种原因已经挂载，直接返回成功
    if (is_sd_mounted == 1) {
        return 0;
    }

    // printf("Waiting for SD Card to mount...\r\n");
    
    // 死等循环，直到超时
    while (SD_Is_Mounted() == 0 && TIMEOUT_COUNT > 0)
    {
        SD_Mount_StateMachine();
        TIMEOUT_COUNT--;
        printf("SD CARD MOUNTTIMEOUT_COUNT: %ld\r\n", TIMEOUT_COUNT);
        // 挂载失败，延时 100ms 后再次尝试，避免疯狂刷总线
        HAL_Delay(100); 
        printf("After Delay\r\n");
    }

    // 5秒循环结束仍未成功，视为未插卡
    // printf("SD Card Mount Timeout (5s)! No card detected.\r\n");
    //is_sd_mounted = 0;
    return SD_Is_Mounted(); // 返回挂载状态
}
/* =======================================================================
 * 普通文件 API 
 * ======================================================================= */
/**
 * @brief 从SD卡文件读取数据
 * @param filename 文件名
 * @param buffer 数据缓冲区，用于存储读取的数据
 * @param length 要读取的数据长度
 * @param bytes_read 实际读取的字节数
 * @return SD_STATUS_OK 读取成功
 * @return SD_STATUS_ERROR 读取失败
 * @note 该函数从文件开头读取数据，最多读取length字节
 */
uint8_t SD_Read_File(const TCHAR *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    // 检查SD卡是否已挂载
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    
    // 以只读模式打开文件
    if (f_open(&NormalFile, filename, FA_READ) == FR_OK) {
        // 读取数据到缓冲区
        f_read(&NormalFile, buffer, length, (UINT*)bytes_read);
        // 关闭文件
        f_close(&NormalFile);
        return SD_STATUS_OK;
    }
    // 操作失败返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 向SD卡文件写入数据
 * @param filename 文件名
 * @param data 要写入的数据缓冲区
 * @param length 要写入的数据长度
 * @return SD_STATUS_OK 写入成功
 * @return SD_STATUS_ERROR 写入失败
 * @note 该函数会覆盖已存在的文件，如果文件不存在则创建
 */
uint8_t SD_Write_File(const TCHAR *filename, uint8_t *data, uint32_t length) {
    uint32_t bw; // 实际写入的字节数
    
    // 检查SD卡是否已挂载
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    
    // 打开文件，FA_WRITE表示写入模式，FA_CREATE_ALWAYS表示总是创建新文件（覆盖已存在的文件）
    if (f_open(&NormalFile, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        // 写入数据
        f_write(&NormalFile, data, length, (UINT*)&bw);
        // 关闭文件
        f_close(&NormalFile);
        // 检查实际写入的字节数是否等于请求写入的字节数
        if (bw == length) return SD_STATUS_OK;
    }
    // 操作失败返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 向SD卡文件追加写入数据
 * @param filename 文件名
 * @param data 要写入的数据缓冲区
 * @param length 要写入的数据长度
 * @return SD_STATUS_OK 写入成功
 * @return SD_STATUS_ERROR 写入失败
 * @note 该函数会在文件末尾追加数据，如果文件不存在则创建
 */
uint8_t SD_Append_File(const TCHAR *filename, uint8_t *data, uint32_t length) {
    uint32_t bw; // 实际写入的字节数
    
    // 检查SD卡是否已挂载
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    
    // 打开文件，FA_WRITE表示写入模式，FA_OPEN_ALWAYS表示如果文件不存在则创建
    if (f_open(&NormalFile, filename, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) {
        // 将文件指针移动到文件末尾
        f_lseek(&NormalFile, f_size(&NormalFile));
        // 写入数据
        f_write(&NormalFile, data, length, (UINT*)&bw);
        // 关闭文件
        f_close(&NormalFile);
        // 检查实际写入的字节数是否等于请求写入的字节数
        if (bw == length) return SD_STATUS_OK;
    }
    // 操作失败返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 删除SD卡上的文件
 * @param filename 要删除的文件名
 * @return SD_STATUS_OK 删除成功
 * @return SD_STATUS_ERROR 删除失败
 * @note 该函数可以删除文件或空目录
 */
uint8_t SD_Delete_File(const TCHAR *filename) {
    // 检查SD卡是否已挂载，并尝试删除文件
    if (!is_sd_mounted || f_unlink(filename) != FR_OK) return SD_STATUS_ERROR;
    
    // 删除成功
    return SD_STATUS_OK;
}

/**
 * @brief 从SD卡文件的指定偏移量读取数据
 * @param filename 文件名
 * @param offset 读取的起始偏移量
 * @param buffer 数据缓冲区，用于存储读取的数据
 * @param length 要读取的数据长度
 * @param bytes_read 实际读取的字节数
 * @return SD_STATUS_OK 读取成功
 * @return SD_STATUS_ERROR 读取失败
 * @note 该函数从文件的指定偏移量开始读取数据，最多读取length字节
 */
uint8_t SD_Read_File_Offset(const TCHAR *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read)
{
    // 检查SD卡是否已挂载
    if (!is_sd_mounted) return SD_STATUS_ERROR;

    // 以只读模式打开文件
    if (f_open(&NormalFile, filename, FA_READ) == FR_OK) 
    {
        // 将文件指针移动到指定偏移量
        if (f_lseek(&NormalFile, offset) == FR_OK) 
        {
            // 读取数据到缓冲区
            f_read(&NormalFile, buffer, length, (UINT*)bytes_read);
            // 关闭文件
            f_close(&NormalFile); 
            return SD_STATUS_OK;
        }
        // 偏移量设置失败时关闭文件
        f_close(&NormalFile); 
    }
    // 操作失败返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 在SD卡上创建目录
 * @param dirname 目录名
 * @return SD_STATUS_OK 创建成功或目录已存在
 * @return SD_STATUS_ERROR 创建失败
 * @note 如果目录已存在，也返回成功状态
 */
uint8_t SD_Create_Dir(const TCHAR *dirname) {
    // 检查SD卡是否已挂载
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    
    // 创建目录
    FRESULT res = f_mkdir(dirname);
    
    // 检查返回结果，FR_OK表示创建成功，FR_EXIST表示目录已存在
    if (res == FR_OK || res == FR_EXIST) return SD_STATUS_OK;
    
    // 操作失败返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 获取SD卡的存储空间信息（以MB为单位）
 * @param total_mb 总空间（MB）
 * @param free_mb 可用空间（MB）
 * @return SD_STATUS_OK 获取成功
 * @return SD_STATUS_ERROR 获取失败
 * @note 该函数通过FatFS的f_getfree函数获取空闲簇数量，然后计算总空间和可用空间
 */
uint8_t SD_Get_Space_MB(uint32_t *total_mb, uint32_t *free_mb) {
    FATFS *pfs; // 文件系统对象指针
    DWORD fre_clust; // 空闲簇数量
    
    // 检查SD卡是否已挂载，并获取空闲簇数量
    if (!is_sd_mounted || f_getfree((TCHAR const*)SDPath, &fre_clust, &pfs) != FR_OK) return SD_STATUS_ERROR;
    
    // 计算总空间：(总簇数 - 2) * 每簇扇区数 / 2048（转换为MB）
    *total_mb = ((pfs->n_fatent - 2) * pfs->csize) / 2048;
    
    // 计算可用空间：空闲簇数 * 每簇扇区数 / 2048（转换为MB）
    *free_mb  = (fre_clust * pfs->csize) / 2048;
    
    // 获取成功
    return SD_STATUS_OK;
}

/**
 * @brief 获取SD卡的存储空间信息（以字节为单位）
 * @param total_bytes 总空间（字节）
 * @param free_bytes 可用空间（字节）
 * @return SD_STATUS_OK 获取成功
 * @return SD_STATUS_ERROR 获取失败
 * @return SD_STATUS_OVERFLOW 空间溢出（超过uint32_t最大值）
 * @note 该函数通过FatFS的f_getfree函数获取空闲簇数量，然后计算总空间和可用空间
 */
uint8_t SD_Get_Space_Bytes(uint32_t *total_bytes, uint32_t *free_bytes) {
    FATFS *pfs; // 文件系统对象指针
    DWORD fre_clust; // 空闲簇数量
    
    // 检查SD卡是否已挂载，并获取空闲簇数量
    if (!is_sd_mounted || f_getfree((TCHAR const*)SDPath, &fre_clust, &pfs) != FR_OK) return SD_STATUS_ERROR;
    
    // 计算总扇区数：(总簇数 - 2) * 每簇扇区数
    uint32_t tot_sect = (pfs->n_fatent - 2) * pfs->csize;
    
    // 检查总扇区数是否会导致uint32_t溢出（每扇区512字节）
    if (tot_sect > (0xFFFFFFFF / 512)) return SD_STATUS_OVERFLOW;
    
    // 计算总空间（字节）：总扇区数 * 512
    *total_bytes = tot_sect * 512;
    
    // 计算可用空间（字节）：空闲簇数 * 每簇扇区数 * 512
    *free_bytes  = fre_clust * pfs->csize * 512;
    
    // 获取成功
    return SD_STATUS_OK;
}

/* =======================================================================
 * 常开槽位 API (核心重构区)
 * ======================================================================= */

/**
 * @brief 打开快速文件槽位
 * @param filename 文件名
 * @return >=0 成功返回槽位索引（0~MAX_SD_FAST_SLOTS-1）
 * @return -1 失败（SD卡未挂载、无空闲槽位或文件打开失败）
 * @note 该函数使用快速槽位缓存打开的文件句柄，提高文件访问效率
 *       使用完毕后需要调用SD_FastSlot_Close关闭槽位
 */
int8_t SD_FastSlot_Open(const TCHAR *filename)
{
    // 检查SD卡是否已挂载
    if (!is_sd_mounted) return -1;

    // 遍历所有快速槽位，查找空闲槽位
    for (int8_t i = 0; i < MAX_SD_FAST_SLOTS; i++) {
        // 检查槽位是否空闲（slot_status[i] == 0表示空闲）
        if (slot_status[i] == 0) {
            // 尝试以只读模式打开文件
            if (f_open(&FastSlots[i], filename, FA_READ) == FR_OK) {
                // 标记槽位为已使用
                slot_status[i] = 1; 
                // 返回槽位索引
                return i;
            }
            // 文件打开失败，返回错误
            return -1; 
        }
    }
    // 没有可用的空闲槽位，返回错误
    return -1; 
}

/**
 * @brief 切换快速槽位到指定的文件
 * @param slot_id 槽位索引（0~MAX_SD_FAST_SLOTS-1）
 * @param filename 要切换到的文件名
 * @return SD_STATUS_OK 切换成功
 * @return SD_STATUS_ERROR 切换失败（SD卡未挂载、槽位索引无效或文件打开失败）
 * @note 该函数会先关闭槽位中当前打开的文件（如果有），然后打开新的文件
 */
uint8_t SD_FastSlot_Switch(int8_t slot_id, const TCHAR *filename)
{
    // 检查SD卡是否已挂载，以及槽位索引是否有效
    if (!is_sd_mounted || slot_id < 0 || slot_id >= MAX_SD_FAST_SLOTS) return SD_STATUS_ERROR;

    // 如果槽位当前已被使用，先关闭已打开的文件
    if (slot_status[slot_id] == 1) {
        f_close(&FastSlots[slot_id]);
        // 标记槽位为空闲
        slot_status[slot_id] = 0;
    }

    // 尝试以只读模式打开新文件
    if (f_open(&FastSlots[slot_id], filename, FA_READ) == FR_OK) {
        // 标记槽位为已使用
        slot_status[slot_id] = 1;
        // 切换成功
        return SD_STATUS_OK;
    }
    
    // 文件打开失败，返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 从快速槽位中读取数据
 * @param slot_id 槽位索引（0~MAX_SD_FAST_SLOTS-1）
 * @param offset 读取的起始偏移量
 * @param buffer 数据缓冲区，用于存储读取的数据
 * @param length 要读取的数据长度
 * @return SD_STATUS_OK 读取成功
 * @return SD_STATUS_ERROR 读取失败（SD卡未挂载、槽位索引无效、槽位未使用或读取失败）
 * @note 该函数从指定槽位的文件中读取数据，需要先使用SD_FastSlot_Open打开文件
 */
uint8_t SD_FastSlot_Read(int8_t slot_id, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    uint32_t bytes_read; // 实际读取的字节数
    
    // 检查SD卡是否已挂载，以及槽位索引是否有效
    if (!is_sd_mounted || slot_id < 0 || slot_id >= MAX_SD_FAST_SLOTS) return SD_STATUS_ERROR;
    
    // 检查槽位是否已被使用（slot_status[slot_id] == 1表示已使用）
    if (slot_status[slot_id] == 0) return SD_STATUS_ERROR; 

    // 将文件指针移动到指定偏移量
    if (f_lseek(&FastSlots[slot_id], offset) == FR_OK) {
        // 读取数据到缓冲区
        if (f_read(&FastSlots[slot_id], buffer, length, (UINT*)&bytes_read) == FR_OK) {
            // 检查实际读取的字节数是否等于请求读取的字节数
            if (bytes_read == length) return SD_STATUS_OK;
        }
    }
    // 操作失败返回错误
    return SD_STATUS_ERROR;
}

/**
 * @brief 关闭快速槽位
 * @param slot_id 槽位索引（0~MAX_SD_FAST_SLOTS-1）
 * @return SD_STATUS_OK 关闭成功
 * @return SD_STATUS_ERROR 关闭失败（槽位索引无效或槽位未使用）
 * @note 该函数关闭指定槽位中的文件并释放槽位资源
 */
uint8_t SD_FastSlot_Close(int8_t slot_id)
{
    // 检查槽位索引是否有效，以及槽位是否已被使用
    if (slot_id >= 0 && slot_id < MAX_SD_FAST_SLOTS && slot_status[slot_id] == 1) {
        // 关闭文件
        f_close(&FastSlots[slot_id]);
        // 标记槽位为空闲
        slot_status[slot_id] = 0; 
        // 关闭成功
        return SD_STATUS_OK;
    }
    // 槽位索引无效或槽位未使用，返回错误
    return SD_STATUS_ERROR;
}


// 内部缓冲区大小，对应 FatFs 的 _MAX_LFN
#define W_PATH_BUF_SIZE 512

/* -----------------------------------------------------------------------
 * 内部工具：UTF-8 转 UTF-16
 * ----------------------------------------------------------------------- */
/**
 * @brief 将UTF-8编码字符串转换为UTF-16编码字符串
 * @param utf8 UTF-8编码的输入字符串
 * @param utf16 UTF-16编码的输出字符串缓冲区
 * @param max_len 输出缓冲区的最大长度（包括终止符）
 * @return 转换后的UTF-16字符串长度（不包括终止符）
 * @note 该函数支持1-3字节的UTF-8编码，4字节的UTF-8编码会被跳过
 *       输出字符串会自动添加终止符'\0'
 */
uint32_t SD_Utils_UTF8ToUTF16(const char* utf8, TCHAR* utf16, uint32_t max_len)
{
    uint32_t i = 0, j = 0; // i: UTF-8字符串索引, j: UTF-16字符串索引
    
    // 遍历UTF-8字符串，直到遇到终止符或输出缓冲区已满
    while (utf8[i] != '\0' && j < (max_len - 1)) {
        unsigned char c = (unsigned char)utf8[i]; // 当前UTF-8字节
        uint32_t uni; // Unicode码点
        
        // 1字节UTF-8编码（0x00-0x7F）：直接转换为Unicode
        if (c < 0x80) { uni = c; i += 1; }
        // 2字节UTF-8编码（0xC0-0xDF）：解码为Unicode
        else if ((c & 0xE0) == 0xC0) { uni = (utf8[i]&0x1F)<<6 | (utf8[i+1]&0x3F); i += 2; }
        // 3字节UTF-8编码（0xE0-0xEF）：解码为Unicode
        else if ((c & 0xF0) == 0xE0) { uni = (utf8[i]&0x0F)<<12 | (utf8[i+1]&0x3F)<<6 | (utf8[i+2]&0x3F); i += 3; }
        // 4字节UTF-8编码（0xF0-0xF7）：跳过（不支持）
        else { i += 4; continue; }
        
        // 将Unicode码点转换为UTF-16字符并存储到输出缓冲区
        utf16[j++] = (TCHAR)uni;
    }
    
    // 添加UTF-16字符串终止符
    utf16[j] = 0;
    
    // 返回转换后的字符串长度
    return j;
}

/* -----------------------------------------------------------------------
 * UTF-8 文件操作函数实现
 * ----------------------------------------------------------------------- */

/**
 * @brief 从SD卡文件读取数据（UTF-8文件名版本）
 * @param filename UTF-8编码的文件名
 * @param buffer 数据缓冲区，用于存储读取的数据
 * @param length 要读取的数据长度
 * @param bytes_read 实际读取的字节数
 * @return SD_STATUS_OK 读取成功
 * @return SD_STATUS_ERROR 读取失败
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_Read_File进行读取
 */
uint8_t SD_Read_File_UTF8(const char *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_Read_File函数进行文件读取
    return SD_Read_File(wPath, buffer, length, bytes_read); 
}

/**
 * @brief 向SD卡文件写入数据（UTF-8文件名版本）
 * @param filename UTF-8编码的文件名
 * @param data 要写入的数据缓冲区
 * @param length 要写入的数据长度
 * @return SD_STATUS_OK 写入成功
 * @return SD_STATUS_ERROR 写入失败
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_Write_File进行写入
 */
uint8_t SD_Write_File_UTF8(const char *filename, uint8_t *data, uint32_t length) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_Write_File函数进行文件写入
    return SD_Write_File(wPath, data, length);
}

/**
 * @brief 向SD卡文件追加写入数据（UTF-8文件名版本）
 * @param filename UTF-8编码的文件名
 * @param data 要写入的数据缓冲区
 * @param length 要写入的数据长度
 * @return SD_STATUS_OK 写入成功
 * @return SD_STATUS_ERROR 写入失败
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_Append_File进行追加写入
 */
uint8_t SD_Append_File_UTF8(const char *filename, uint8_t *data, uint32_t length) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_Append_File函数进行文件追加写入
    return SD_Append_File(wPath, data, length);
}

/**
 * @brief 删除SD卡上的文件（UTF-8文件名版本）
 * @param filename UTF-8编码的文件名
 * @return SD_STATUS_OK 删除成功
 * @return SD_STATUS_ERROR 删除失败
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_Delete_File进行删除
 */
uint8_t SD_Delete_File_UTF8(const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_Delete_File函数进行文件删除
    return SD_Delete_File(wPath);
}

/**
 * @brief 在SD卡上创建目录（UTF-8目录名版本）
 * @param dirname UTF-8编码的目录名
 * @return SD_STATUS_OK 创建成功或目录已存在
 * @return SD_STATUS_ERROR 创建失败
 * @note 该函数将UTF-8编码的目录名转换为UTF-16编码，然后调用SD_Create_Dir进行创建
 */
uint8_t SD_Create_Dir_UTF8(const char *dirname) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的目录路径缓冲区
    
    // 将UTF-8编码的目录名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(dirname, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_Create_Dir函数进行目录创建
    return SD_Create_Dir(wPath);
}

/**
 * @brief 从SD卡文件的指定偏移量读取数据（UTF-8文件名版本）
 * @param filename UTF-8编码的文件名
 * @param offset 读取的起始偏移量
 * @param buffer 数据缓冲区，用于存储读取的数据
 * @param length 要读取的数据长度
 * @param bytes_read 实际读取的字节数
 * @return SD_STATUS_OK 读取成功
 * @return SD_STATUS_ERROR 读取失败
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_Read_File_Offset进行读取
 */
uint8_t SD_Read_File_Offset_UTF8(const char *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_Read_File_Offset函数进行文件偏移读取
    return SD_Read_File_Offset(wPath, offset, buffer, length, bytes_read);
}

/* -----------------------------------------------------------------------
 * UTF-8 槽位操作函数实现
 * ----------------------------------------------------------------------- */

/**
 * @brief 打开快速文件槽位（UTF-8文件名版本）
 * @param filename UTF-8编码的文件名
 * @return >=0 成功返回槽位索引（0~MAX_SD_FAST_SLOTS-1）
 * @return -1 失败（SD卡未挂载、无空闲槽位或文件打开失败）
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_FastSlot_Open打开槽位
 *       使用完毕后需要调用SD_FastSlot_Close关闭槽位
 */
int8_t SD_FastSlot_Open_UTF8(const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_FastSlot_Open函数打开快速槽位
    return SD_FastSlot_Open(wPath);
}

/**
 * @brief 切换快速槽位到指定的文件（UTF-8文件名版本）
 * @param slot_id 槽位索引（0~MAX_SD_FAST_SLOTS-1）
 * @param filename UTF-8编码的文件名
 * @return SD_STATUS_OK 切换成功
 * @return SD_STATUS_ERROR 切换失败（SD卡未挂载、槽位索引无效或文件打开失败）
 * @note 该函数将UTF-8编码的文件名转换为UTF-16编码，然后调用SD_FastSlot_Switch进行切换
 */
uint8_t SD_FastSlot_Switch_UTF8(int8_t slot_id, const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE]; // UTF-16编码的文件路径缓冲区
    
    // 将UTF-8编码的文件名转换为UTF-16编码
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    
    // 调用底层SD_FastSlot_Switch函数进行槽位切换
    return SD_FastSlot_Switch(slot_id, wPath);
}