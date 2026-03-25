/**
  ******************************************************************************
  * @file    sd_app.h
  * @brief   SD/TF卡应用层底层驱动 (支持 N 个常开槽位与动态切换)
  * 架构完全兼容 USB 应用层驱动
  ******************************************************************************
  */
#ifndef __SD_APP_H
#define __SD_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fatfs.h" // CubeMX 生成的 FatFs 配置文件，包含了 ff.h 等

/* ================= 1. 核心宏定义 ================= */
#define SD_USE_RTOS          1     // 1: 启用 FreeRTOS 支持, 0: 裸机模式
#define MAX_SD_FAST_SLOTS    10    // 定义系统支持的最大常开文件槽位数量 (请确保 FATFS 设置里的 _FS_LOCK 不小于此数)

#if SD_USE_RTOS == 1
  #include "cmsis_os.h"
#endif

/* ================= 2. 状态码定义 ================= */
#define SD_STATUS_OK         0x00  // 操作成功
#define SD_STATUS_ERROR      0x01  // 操作失败
#define SD_STATUS_OVERFLOW   0x02  // 数值溢出

/* ================= 3. 后台任务与状态 ================= */
uint8_t SD_Is_Mounted(void);

#if SD_USE_RTOS == 1
  void SD_App_Thread(void); 
#else
  void SD_App_Process(void);                 
#endif

/* ================= 4. 普通文件 API (低频、安全) ================= */
uint8_t SD_Read_File(const TCHAR *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read);
uint8_t SD_Write_File(const TCHAR *filename, uint8_t *data, uint32_t length);
uint8_t SD_Append_File(const TCHAR *filename, uint8_t *data, uint32_t length);
uint8_t SD_Delete_File(const TCHAR *filename);
uint8_t SD_Create_Dir(const TCHAR *dirname);
// 普通文件：从指定偏移位置读取数据
uint8_t SD_Read_File_Offset(const TCHAR *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read);

/* ================= 5. 容量获取 API  ================= */
uint8_t SD_Get_Space_Bytes(uint32_t *total_bytes, uint32_t *free_bytes);
uint8_t SD_Get_Space_MB(uint32_t *total_mb, uint32_t *free_mb);

/* ================= 6. 常开槽位 API (高频极速、动态切换) ================= */
// 自动寻找空闲槽位并打开文件
int8_t  SD_FastSlot_Open(const TCHAR *filename);

// 强制在指定槽位打开文件 (如果槽位已被占用，会自动先关闭旧文件)
uint8_t SD_FastSlot_Switch(int8_t slot_id, const TCHAR *filename);

// 极速读取指定槽位的文件，通过偏移量读取数据
uint8_t SD_FastSlot_Read(int8_t slot_id, uint32_t offset, uint8_t *buffer, uint32_t length);

// 关闭指定槽位
uint8_t SD_FastSlot_Close(int8_t slot_id);

/*================== 7. UTF-8 风格的文件 API ================= */
/* ================= 7.1. 内部转换工具 ================= */
// 将 UTF-8 字符串转换为 UTF-16 (FatFs 所需)
uint32_t SD_Utils_UTF8ToUTF16(const char* utf8, TCHAR* utf16, uint32_t max_len);

/* ================= 7.2. UTF-8 风格的普通文件 API ================= */
uint8_t SD_Read_File_UTF8(const char *filename, uint8_t *buffer, uint32_t length, uint32_t *bytes_read);
uint8_t SD_Write_File_UTF8(const char *filename, uint8_t *data, uint32_t length);
uint8_t SD_Append_File_UTF8(const char *filename, uint8_t *data, uint32_t length);
uint8_t SD_Delete_File_UTF8(const char *filename);
uint8_t SD_Create_Dir_UTF8(const char *dirname);
uint8_t SD_Read_File_Offset_UTF8(const char *filename, uint32_t offset, uint8_t *buffer, uint32_t length, uint32_t *bytes_read);

/* ================= 7.3. UTF-8 风格的常开槽位 API ================= */
int8_t  SD_FastSlot_Open_UTF8(const char *filename);
uint8_t SD_FastSlot_Switch_UTF8(int8_t slot_id, const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* __SD_APP_H */