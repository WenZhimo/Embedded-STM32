#ifndef __NDEF_H
#define __NDEF_H

#include "rc522.h"
#include <stdint.h>
#include <string.h>

// NDEF 记录类型枚举
typedef enum {
    NDEF_TYPE_TEXT = 0,
    NDEF_TYPE_URI  = 1
} NDEF_RecordType;

// URI 前缀标识码 (NFC Forum 规定)
#define URI_PREFIX_NONE         0x00
#define URI_PREFIX_HTTP_WWWDOT  0x01 // http://www.
#define URI_PREFIX_HTTPS_WWWDOT 0x02 // https://www.
#define URI_PREFIX_HTTP         0x03 // http://
#define URI_PREFIX_HTTPS        0x04 // https://

// 单个 NDEF 记录结构体
/*
    * NDEF_Record: 表示一条 NDEF 记录
    * - type: 记录类型 (URI 或 Text)
    * - uriCode: URI 记录的前缀码 (仅当 type=URI 时有效)
    * - language: Text 记录的语言代码 (仅当 type=Text 时有效)
    * - payload: 记录的载荷数据 (URI 的 URL 字符串或 Text 的文本内容)
    * - payloadLength: 载荷数据的实际长度 (字节数)
*/
typedef struct {
    NDEF_RecordType type;        
    uint8_t uriCode;             // 仅 URI 使用
    char language[3];            // 仅 Text 使用
    uint8_t payload[48];         // 载荷数据 (可根据单片机RAM调整大小)
    uint8_t payloadLength;       
} NDEF_Record;

// 限制一条消息最多包含的记录数，防止内存溢出
#define NDEF_MAX_RECORDS 8 


/*
 * NDEF_Message: 包含多个 NDEF_Record 的结构体
 * - records: 记录数组，最多包含 NDEF_MAX_RECORDS 个记录
 * - recordCount: 当前消息中包含的有效记录数量
 */
typedef struct {
    NDEF_Record records[NDEF_MAX_RECORDS]; // 记录数组
    uint8_t recordCount;                   // 当前包含的记录数量
} NDEF_Message;

// API 函数声明

// 初始化 NDEF 消息结构体，清空之前的记录
void NDEF_Message_Init(NDEF_Message *msg);
// 向消息中添加一条 URI (网址) 记录
int8_t NDEF_AddUriRecord(NDEF_Message *msg, uint8_t uriCode, const char *url);
// 向消息中添加一条 Text (纯文本) 记录
int8_t NDEF_AddTextRecord(NDEF_Message *msg, const char *language, const char *text);


// 向 M1 卡写入包含多条记录的 NDEF 消息
int8_t NDEF_WriteToCard(uint8_t *uid, NDEF_Message *msg);
// 从 M1 卡读取 NDEF 消息并解析成结构体
int8_t NDEF_ReadFromCard(uint8_t *uid, NDEF_Message *msg);


// 将出厂白卡初始化为标准 NDEF 卡片 (包含 MAD 目录)
int8_t NDEF_FormatBlankCard(uint8_t *uid);
// 自适应识别卡片状态并再格式化为 空白 NDEF 卡
int8_t NDEF_FormatUniversal(uint8_t *uid);


#endif