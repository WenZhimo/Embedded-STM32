/**
  ******************************************************************************
  * @file    usb_app.c
  * @brief   U盘应用层底层驱动实现 (带常开槽位管理)
  ******************************************************************************
  */
#include "usb_app.h"
#include "../../ThirdParty/MyLib/EUBF/eubf_manager.h"
#include <string.h>

extern ApplicationTypeDef Appli_state;
extern char USBHPath[4];

/* 核心文件系统对象 */
FATFS USBDISKFatFs;            
FIL NormalFile;                

/* --- 常开槽位管理池 --- */
FIL FastSlots[MAX_FAST_SLOTS];               // 槽位文件对象数组
uint8_t slot_status[MAX_FAST_SLOTS] = {0};   // 槽位状态：0为空闲，1为占用

uint8_t is_usb_mounted = 0;    

uint8_t USB_Is_Mounted(void) {
    return is_usb_mounted;
}

/* =======================================================================
 * 后台挂载管理与紧急清理
 * ======================================================================= */
static void USB_Mount_StateMachine(void)
{
    if (Appli_state == APPLICATION_READY) 
    {
        if (is_usb_mounted == 0) 
        {
            if (f_mount(&USBDISKFatFs, (TCHAR const*)USBHPath, 1) == FR_OK) {
                is_usb_mounted = 1;
            }
        }
    }
    else if (Appli_state == APPLICATION_DISCONNECT)
    {
        if (is_usb_mounted == 1)
        {
            // 【重要】：U盘拔出时，紧急关闭所有活跃的槽位
            for (int i = 0; i < MAX_FAST_SLOTS; i++) {
                if (slot_status[i] == 1) {
                    f_close(&FastSlots[i]);
                    slot_status[i] = 0; // 释放槽位
                }
            }
            // 通知字库管理器：之前的缓存引用全部失效了！
            EUBF_Notify_USB_Disconnected();
            // 卸载文件系统
            f_mount(NULL, (TCHAR const*)USBHPath, 0); 
            is_usb_mounted = 0;
        }
    }
}

#if USB_USE_RTOS == 1
void USB_App_Thread(void const * argument) {
    for(;;) {
        USB_Mount_StateMachine();
        osDelay(100); 
    }
}
#else
void USB_App_Process(void) {
    USB_Mount_StateMachine();
}
#endif

/* =======================================================================
 * 普通文件 API (略作折叠，逻辑与之前完全一致)
 * ======================================================================= */
uint8_t USB_Read_File(const TCHAR *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    if (!is_usb_mounted) return USB_STATUS_ERROR;
    if (f_open(&NormalFile, filename, FA_READ) == FR_OK) {
        f_read(&NormalFile, buffer, length, (UINT*)bytes_read);
        f_close(&NormalFile);
        return USB_STATUS_OK;
    }
    return USB_STATUS_ERROR;
}

uint8_t USB_Write_File(const TCHAR *filename, uint8_t *data, uint32_t length) {
    uint32_t bw;
    if (!is_usb_mounted) return USB_STATUS_ERROR;
    if (f_open(&NormalFile, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        f_write(&NormalFile, data, length, (UINT*)&bw);
        f_close(&NormalFile);
        if (bw == length) return USB_STATUS_OK;
    }
    return USB_STATUS_ERROR;
}

uint8_t USB_Append_File(const TCHAR *filename, uint8_t *data, uint32_t length) {
    uint32_t bw;
    if (!is_usb_mounted) return USB_STATUS_ERROR;
    if (f_open(&NormalFile, filename, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) {
        f_lseek(&NormalFile, f_size(&NormalFile));
        f_write(&NormalFile, data, length, (UINT*)&bw);
        f_close(&NormalFile);
        if (bw == length) return USB_STATUS_OK;
    }
    return USB_STATUS_ERROR;
}

uint8_t USB_Delete_File(const TCHAR *filename) {
    if (!is_usb_mounted || f_unlink(filename) != FR_OK) return USB_STATUS_ERROR;
    return USB_STATUS_OK;
}


/**
  * @brief  从普通文件的指定偏移位置读取数据 (低频使用，用完即关)
  * @param  filename: 文件名
  * @param  offset:   要读取的起始偏移量
  * @param  buffer:   数据缓冲区
  * @param  length:   要读取的长度
  * @param  bytes_read: 实际读到的长度
  * @retval 0: 成功, 1: 失败
  */
uint8_t USB_Read_File_Offset(const TCHAR *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read)
{
    if (!is_usb_mounted) return USB_STATUS_ERROR;

    // 1. 打开文件
    if (f_open(&NormalFile, filename, FA_READ) == FR_OK) 
    {
        // 2. 移动指针到指定偏移量
        if (f_lseek(&NormalFile, offset) == FR_OK) 
        {
            // 3. 读取数据
            f_read(&NormalFile, buffer, length, (UINT*)bytes_read);
            
            f_close(&NormalFile); // 用完立即安全关闭
            return USB_STATUS_OK;
        }
        f_close(&NormalFile); // 如果寻址失败，也要记得关闭文件！
    }
    return USB_STATUS_ERROR;
}



uint8_t USB_Create_Dir(const TCHAR *dirname) {
    if (!is_usb_mounted) return USB_STATUS_ERROR;
    FRESULT res = f_mkdir(dirname);
    if (res == FR_OK || res == FR_EXIST) return USB_STATUS_OK;
    return USB_STATUS_ERROR;
}

uint8_t USB_Get_Space_MB(uint32_t *total_mb, uint32_t *free_mb) {
    FATFS *pfs; DWORD fre_clust;
    if (!is_usb_mounted || f_getfree((TCHAR const*)USBHPath, &fre_clust, &pfs) != FR_OK) return USB_STATUS_ERROR;
    *total_mb = ((pfs->n_fatent - 2) * pfs->csize) / 2048;
    *free_mb  = (fre_clust * pfs->csize) / 2048;
    return USB_STATUS_OK;
}

uint8_t USB_Get_Space_Bytes(uint32_t *total_bytes, uint32_t *free_bytes) {
    FATFS *pfs; DWORD fre_clust;
    if (!is_usb_mounted || f_getfree((TCHAR const*)USBHPath, &fre_clust, &pfs) != FR_OK) return USB_STATUS_ERROR;
    uint32_t tot_sect = (pfs->n_fatent - 2) * pfs->csize;
    if (tot_sect > (0xFFFFFFFF / 512)) return USB_STATUS_OVERFLOW;
    *total_bytes = tot_sect * 512;
    *free_bytes  = fre_clust * pfs->csize * 512;
    return USB_STATUS_OK;
}

/* =======================================================================
 * 常开槽位 API (核心重构区)
 * ======================================================================= */

/**
  * @brief  自动寻找空闲槽位打开文件
  * @retval >=0: 分配到的槽位ID, -1: 失败(无空闲槽位或文件不存在)
  */
int8_t USB_FastSlot_Open(const TCHAR *filename)
{
    if (!is_usb_mounted) return -1;

    for (int8_t i = 0; i < MAX_FAST_SLOTS; i++) {
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

/**
  * @brief  在指定槽位强行打开文件 (核心动态切换 API)
  * @note   如果该槽位原来有文件开着，会自动安全关闭旧文件并打开新文件
  * @retval 0: 成功, 1: 失败
  */
uint8_t USB_FastSlot_Switch(int8_t slot_id, const TCHAR *filename)
{
    if (!is_usb_mounted || slot_id < 0 || slot_id >= MAX_FAST_SLOTS) return USB_STATUS_ERROR;

    // 1. 如果该槽位被占用，先安全关闭它
    if (slot_status[slot_id] == 1) {
        f_close(&FastSlots[slot_id]);
        slot_status[slot_id] = 0;
    }

    // 2. 在该槽位打开新文件
    if (f_open(&FastSlots[slot_id], filename, FA_READ) == FR_OK) {
        slot_status[slot_id] = 1;
        return USB_STATUS_OK;
    }
    
    return USB_STATUS_ERROR;
}

/**
  * @brief  通过槽位ID极速读取数据
  */
uint8_t USB_FastSlot_Read(int8_t slot_id, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    uint32_t bytes_read;
    
    if (!is_usb_mounted || slot_id < 0 || slot_id >= MAX_FAST_SLOTS) return USB_STATUS_ERROR;
    if (slot_status[slot_id] == 0) return USB_STATUS_ERROR; // 槽位是空的

    if (f_lseek(&FastSlots[slot_id], offset) == FR_OK) {
        if (f_read(&FastSlots[slot_id], buffer, length, (UINT*)&bytes_read) == FR_OK) {
            if (bytes_read == length) return USB_STATUS_OK;
        }
    }
    return USB_STATUS_ERROR;
}

/**
  * @brief  手动关闭指定槽位
  */
uint8_t USB_FastSlot_Close(int8_t slot_id)
{
    if (slot_id >= 0 && slot_id < MAX_FAST_SLOTS && slot_status[slot_id] == 1) {
        f_close(&FastSlots[slot_id]);
        slot_status[slot_id] = 0; 
        return USB_STATUS_OK;
    }
    return USB_STATUS_ERROR;
}




// 内部缓冲区大小，对应 FatFs 的 _MAX_LFN
#define W_PATH_BUF_SIZE 512

/* -----------------------------------------------------------------------
 * 内部工具：UTF-8 转 UTF-16
 * ----------------------------------------------------------------------- */
uint32_t USB_Utils_UTF8ToUTF16(const char* utf8, TCHAR* utf16, uint32_t max_len)
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

uint8_t USB_Read_File_UTF8(const char *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_Read_File(wPath, buffer, length, bytes_read); // 调用原始 TCHAR 接口
}

uint8_t USB_Write_File_UTF8(const char *filename, uint8_t *data, uint32_t length) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_Write_File(wPath, data, length);
}

uint8_t USB_Append_File_UTF8(const char *filename, uint8_t *data, uint32_t length) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_Append_File(wPath, data, length);
}

uint8_t USB_Delete_File_UTF8(const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_Delete_File(wPath);
}

uint8_t USB_Create_Dir_UTF8(const char *dirname) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(dirname, wPath, W_PATH_BUF_SIZE);
    return USB_Create_Dir(wPath);
}

uint8_t USB_Read_File_Offset_UTF8(const char *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_Read_File_Offset(wPath, offset, buffer, length, bytes_read);
}

/* -----------------------------------------------------------------------
 * UTF-8 槽位操作函数实现
 * ----------------------------------------------------------------------- */

int8_t USB_FastSlot_Open_UTF8(const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_FastSlot_Open(wPath);
}

uint8_t USB_FastSlot_Switch_UTF8(int8_t slot_id, const char *filename) {
    TCHAR wPath[W_PATH_BUF_SIZE];
    USB_Utils_UTF8ToUTF16(filename, wPath, W_PATH_BUF_SIZE);
    return USB_FastSlot_Switch(slot_id, wPath);
}