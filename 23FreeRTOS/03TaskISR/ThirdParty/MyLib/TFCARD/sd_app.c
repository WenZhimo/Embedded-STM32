/**
  ******************************************************************************
  * @file    sd_app.c
  * @brief   SD/TF卡应用层底层驱动实现 (带常开槽位管理，适配无CD引脚硬件)
  ******************************************************************************
  */
#include "sd_app.h"
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
static void SD_Mount_StateMachine(void)
{
    /* * 由于硬件没有 Card Detect (CD) 引脚，我们采用“挂载即常驻”的策略。
     * 只要 is_sd_mounted 为 0，就不断尝试挂载。一旦挂载成功，就假设卡一直在线。
     */
    if (is_sd_mounted == 0) 
    {
        // 第三个参数为 1，表示立即尝试挂载，这样如果卡没插或者初始化失败，会立刻返回错误
        if (f_mount(&SDFatFs, (TCHAR const*)SDPath, 1) == FR_OK) {
            is_sd_mounted = 1;
            // 可选：在这里可以通过串口打印 "SD Card Mounted Successfully"
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

/* =======================================================================
 * 普通文件 API 
 * ======================================================================= */
uint8_t SD_Read_File(const TCHAR *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    if (f_open(&NormalFile, filename, FA_READ) == FR_OK) {
        f_read(&NormalFile, buffer, length, (UINT*)bytes_read);
        f_close(&NormalFile);
        return SD_STATUS_OK;
    }
    return SD_STATUS_ERROR;
}

uint8_t SD_Write_File(const TCHAR *filename, uint8_t *data, uint32_t length) {
    uint32_t bw;
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    if (f_open(&NormalFile, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        f_write(&NormalFile, data, length, (UINT*)&bw);
        f_close(&NormalFile);
        if (bw == length) return SD_STATUS_OK;
    }
    return SD_STATUS_ERROR;
}

uint8_t SD_Append_File(const TCHAR *filename, uint8_t *data, uint32_t length) {
    uint32_t bw;
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    if (f_open(&NormalFile, filename, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) {
        f_lseek(&NormalFile, f_size(&NormalFile));
        f_write(&NormalFile, data, length, (UINT*)&bw);
        f_close(&NormalFile);
        if (bw == length) return SD_STATUS_OK;
    }
    return SD_STATUS_ERROR;
}

uint8_t SD_Delete_File(const TCHAR *filename) {
    if (!is_sd_mounted || f_unlink(filename) != FR_OK) return SD_STATUS_ERROR;
    return SD_STATUS_OK;
}

uint8_t SD_Read_File_Offset(const TCHAR *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read)
{
    if (!is_sd_mounted) return SD_STATUS_ERROR;

    if (f_open(&NormalFile, filename, FA_READ) == FR_OK) 
    {
        if (f_lseek(&NormalFile, offset) == FR_OK) 
        {
            f_read(&NormalFile, buffer, length, (UINT*)bytes_read);
            f_close(&NormalFile); 
            return SD_STATUS_OK;
        }
        f_close(&NormalFile); 
    }
    return SD_STATUS_ERROR;
}

uint8_t SD_Create_Dir(const TCHAR *dirname) {
    if (!is_sd_mounted) return SD_STATUS_ERROR;
    FRESULT res = f_mkdir(dirname);
    if (res == FR_OK || res == FR_EXIST) return SD_STATUS_OK;
    return SD_STATUS_ERROR;
}

uint8_t SD_Get_Space_MB(uint32_t *total_mb, uint32_t *free_mb) {
    FATFS *pfs; DWORD fre_clust;
    if (!is_sd_mounted || f_getfree((TCHAR const*)SDPath, &fre_clust, &pfs) != FR_OK) return SD_STATUS_ERROR;
    *total_mb = ((pfs->n_fatent - 2) * pfs->csize) / 2048;
    *free_mb  = (fre_clust * pfs->csize) / 2048;
    return SD_STATUS_OK;
}

uint8_t SD_Get_Space_Bytes(uint32_t *total_bytes, uint32_t *free_bytes) {
    FATFS *pfs; DWORD fre_clust;
    if (!is_sd_mounted || f_getfree((TCHAR const*)SDPath, &fre_clust, &pfs) != FR_OK) return SD_STATUS_ERROR;
    uint32_t tot_sect = (pfs->n_fatent - 2) * pfs->csize;
    if (tot_sect > (0xFFFFFFFF / 512)) return SD_STATUS_OVERFLOW;
    *total_bytes = tot_sect * 512;
    *free_bytes  = fre_clust * pfs->csize * 512;
    return SD_STATUS_OK;
}

/* =======================================================================
 * 常开槽位 API (核心重构区)
 * ======================================================================= */

int8_t SD_FastSlot_Open(const TCHAR *filename)
{
    if (!is_sd_mounted) return -1;

    for (int8_t i = 0; i < MAX_SD_FAST_SLOTS; i++) {
        if (slot_status[i] == 0) {
            if (f_open(&FastSlots[i], filename, FA_READ) == FR_OK) {
                slot_status[i] = 1; 
                return i;
            }
            return -1; 
        }
    }
    return -1; 
}

uint8_t SD_FastSlot_Switch(int8_t slot_id, const TCHAR *filename)
{
    if (!is_sd_mounted || slot_id < 0 || slot_id >= MAX_SD_FAST_SLOTS) return SD_STATUS_ERROR;

    if (slot_status[slot_id] == 1) {
        f_close(&FastSlots[slot_id]);
        slot_status[slot_id] = 0;
    }

    if (f_open(&FastSlots[slot_id], filename, FA_READ) == FR_OK) {
        slot_status[slot_id] = 1;
        return SD_STATUS_OK;
    }
    
    return SD_STATUS_ERROR;
}

uint8_t SD_FastSlot_Read(int8_t slot_id, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    uint32_t bytes_read;
    
    if (!is_sd_mounted || slot_id < 0 || slot_id >= MAX_SD_FAST_SLOTS) return SD_STATUS_ERROR;
    if (slot_status[slot_id] == 0) return SD_STATUS_ERROR; 

    if (f_lseek(&FastSlots[slot_id], offset) == FR_OK) {
        if (f_read(&FastSlots[slot_id], buffer, length, (UINT*)&bytes_read) == FR_OK) {
            if (bytes_read == length) return SD_STATUS_OK;
        }
    }
    return SD_STATUS_ERROR;
}

uint8_t SD_FastSlot_Close(int8_t slot_id)
{
    if (slot_id >= 0 && slot_id < MAX_SD_FAST_SLOTS && slot_status[slot_id] == 1) {
        f_close(&FastSlots[slot_id]);
        slot_status[slot_id] = 0; 
        return SD_STATUS_OK;
    }
    return SD_STATUS_ERROR;
}


// 内部缓冲区大小，对应 FatFs 的 _MAX_LFN
#define W_PATH_BUF_SIZE 512

/* -----------------------------------------------------------------------
 * 内部工具：UTF-8 转 UTF-16
 * ----------------------------------------------------------------------- */
uint32_t SD_Utils_UTF8ToUTF16(const char* utf8, TCHAR* utf16, uint32_t max_len)
{
    uint32_t i = 0, j = 0;
    while (utf8[i] != '\0' && j < (max_len - 1)) {
        unsigned char c = (unsigned char)utf8[i];
        uint32_t uni;
        if (c < 0x80) { uni = c; i += 1; }
        else if ((c & 0xE0) == 0xC0) { uni = (utf8[i]&0x1F)<<6 | (utf8[i+1]&0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0) { uni = (utf8[i]&0x0F)<<12 | (utf8[i+1]&0x3F)<<6 | (utf8[i+2]&0x3F); i += 3; }
        else { i += 4; continue; }
        utf16[j++] = (TCHAR)uni;
    }
    utf16[j] = 0;
    return j;
}

/* -----------------------------------------------------------------------
 * UTF-8 文件操作函数实现
 * ----------------------------------------------------------------------- */

uint8_t SD_Read_File_UTF8(const char *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_Read_File(wPath, buffer, length, bytes_read); 
}

uint8_t SD_Write_File_UTF8(const char *filename, uint8_t *data, uint32_t length) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_Write_File(wPath, data, length);
}

uint8_t SD_Append_File_UTF8(const char *filename, uint8_t *data, uint32_t length) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_Append_File(wPath, data, length);
}

uint8_t SD_Delete_File_UTF8(const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_Delete_File(wPath);
}

uint8_t SD_Create_Dir_UTF8(const char *dirname) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(dirname, wPath, W_PATH_BUF_SIZE);
    return SD_Create_Dir(wPath);
}

uint8_t SD_Read_File_Offset_UTF8(const char *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_Read_File_Offset(wPath, offset, buffer, length, bytes_read);
}

/* -----------------------------------------------------------------------
 * UTF-8 槽位操作函数实现
 * ----------------------------------------------------------------------- */

int8_t SD_FastSlot_Open_UTF8(const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_FastSlot_Open(wPath);
}

uint8_t SD_FastSlot_Switch_UTF8(int8_t slot_id, const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    SD_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return SD_FastSlot_Switch(slot_id, wPath);
}