#include "ndef.h"

#define NDEF_MAX_BUFFER_SIZE 256 
#define NDEF_START_BLOCK     4   


/**
 * @brief  [内部函数] 获取操作特定块所需的密钥
 */
static uint8_t* GetKeyForBlock(uint8_t absBlock)
{
    uint8_t sector = absBlock / 4;
    
    // 扇区 0 是 MAD 目录，使用公开目录密钥 Key_MAD (A0 A1 A2 A3 A4 A5)
    if (sector == 0) return Key_MAD;
    
    // 扇区 1~15 全是 NDEF 数据存放区，必须统一使用 Key_NDEF (D3 F7 D3 F7 D3 F7)
    return Key_NDEF;
}

// ======================= 配置与添加记录 API =======================

/**
 * @brief  初始化 NDEF 消息结构体，清空之前的记录
 */
void NDEF_Message_Init(NDEF_Message *msg) 
{
    memset(msg, 0, sizeof(NDEF_Message));
}

/**
 * @brief  向消息中添加一条 URI (网址) 记录
 */
int8_t NDEF_AddUriRecord(NDEF_Message *msg, uint8_t uriCode, const char *url) 
{
    if (msg->recordCount >= NDEF_MAX_RECORDS) return MI_ERR; // 超出最大记录数限制
    
    NDEF_Record *rec = &msg->records[msg->recordCount];
    rec->type = NDEF_TYPE_URI;
    rec->uriCode = uriCode;
    rec->payloadLength = strlen(url);
    
    if (rec->payloadLength > sizeof(rec->payload)) return MI_ERR; // 长度溢出保护
    
    memcpy(rec->payload, url, rec->payloadLength);
    msg->recordCount++;
    
    return MI_OK;
}

/**
 * @brief  向消息中添加一条 Text (纯文本) 记录
 */
int8_t NDEF_AddTextRecord(NDEF_Message *msg, const char *language, const char *text) 
{
    if (msg->recordCount >= NDEF_MAX_RECORDS) return MI_ERR;
    
    NDEF_Record *rec = &msg->records[msg->recordCount];
    rec->type = NDEF_TYPE_TEXT;
    
    strncpy(rec->language, language, 2);
    rec->language[2] = '\0';
    
    rec->payloadLength = strlen(text);
    if (rec->payloadLength > sizeof(rec->payload)) return MI_ERR;
    
    memcpy(rec->payload, text, rec->payloadLength);
    msg->recordCount++;
    
    return MI_OK;
}

// ======================= 读写底层实现 =======================

/**
 * @brief  向 M1 卡写入包含多条记录的 NDEF 消息
 */
int8_t NDEF_WriteToCard(uint8_t *uid, NDEF_Message *msg)
{
    if (msg->recordCount == 0) return MI_ERR; // 空消息拦截

    uint8_t buffer[NDEF_MAX_BUFFER_SIZE];
    uint8_t recordBuf[NDEF_MAX_BUFFER_SIZE - 4]; // 用于缓存所有记录的内容
    memset(buffer, 0, sizeof(buffer));
    
    uint16_t rIdx = 0; // 记录缓冲区偏移

    // 1. 遍历组装所有 NDEF Records
    for (uint8_t i = 0; i < msg->recordCount; i++) 
    {
        NDEF_Record *rec = &msg->records[i];
        
        // 生成 Header: SR=1 (短载荷), TNF=1 (Well-Known) -> 0x11
        uint8_t header = 0x11; 
        if (i == 0) header |= 0x80;                        // MB=1 (Message Begin)
        if (i == msg->recordCount - 1) header |= 0x40;     // ME=1 (Message End)
        
        recordBuf[rIdx++] = header;
        
        if (rec->type == NDEF_TYPE_URI) {
            recordBuf[rIdx++] = 0x01; // Type Length ('U')
            recordBuf[rIdx++] = rec->payloadLength + 1; // Payload Length
            recordBuf[rIdx++] = 'U';  
            recordBuf[rIdx++] = rec->uriCode; 
            memcpy(&recordBuf[rIdx], rec->payload, rec->payloadLength);
            rIdx += rec->payloadLength;
        } 
        else if (rec->type == NDEF_TYPE_TEXT) {
            uint8_t langLen = strlen(rec->language);
            if (langLen == 0) langLen = 2; 
            
            recordBuf[rIdx++] = 0x01; // Type Length ('T')
            recordBuf[rIdx++] = rec->payloadLength + langLen + 1; 
            recordBuf[rIdx++] = 'T';  
            recordBuf[rIdx++] = langLen; // 状态字节 
            memcpy(&recordBuf[rIdx], rec->language, langLen);
            rIdx += langLen;
            memcpy(&recordBuf[rIdx], rec->payload, rec->payloadLength);
            rIdx += rec->payloadLength;
        }
    }

    // 2. 将组装好的记录打包进 NDEF TLV (Type-Length-Value)
    uint16_t idx = 0;
    buffer[idx++] = 0x03; // Type: NDEF Message
    buffer[idx++] = (uint8_t)rIdx; // Length: 记录总长度
    memcpy(&buffer[idx], recordBuf, rIdx); // Value: 记录数据
    idx += rIdx;
    buffer[idx++] = 0xFE; // Type: Terminator

    // 3. 物理分块写入卡片逻辑 (与原先一致)
    uint8_t blocksNeeded = (idx % 16 == 0) ? (idx / 16) : (idx / 16 + 1);
    uint8_t currentAbsBlock = NDEF_START_BLOCK;
    uint8_t writeBuf[16];
    int8_t status;

    for (uint8_t b = 0; b < blocksNeeded; b++) 
    {
        if (currentAbsBlock % 4 == 3) currentAbsBlock++; // 避让控制块

        memset(writeBuf, 0, 16);
        uint8_t copyLen = (idx - b * 16) > 16 ? 16 : (idx - b * 16);
        memcpy(writeBuf, &buffer[b * 16], copyLen);

        uint8_t *keyToUse = GetKeyForBlock(currentAbsBlock);
        status = M1_WriteDataBlock_Abs(uid, currentAbsBlock, keyToUse, writeBuf);
        if (status != MI_OK) return status;

        #ifdef M1_AUTO_HALT_ENABLE
        uint8_t tagType[2];
        PcdRequest(PICC_REQALL, tagType); 
        PcdAnticoll(uid);
        #endif
        
        currentAbsBlock++;
    }

    return MI_OK;
}

/**
 * @brief  从 M1 卡读取并解析多记录的 NDEF 消息
 */
/**
 * @brief  从 M1 卡读取并解析多记录的 NDEF 消息
 */
int8_t NDEF_ReadFromCard(uint8_t *uid, NDEF_Message *msg)
{
    uint8_t readBuf[16];
    int8_t status;
    uint8_t currentAbsBlock = NDEF_START_BLOCK;
    
    // ==========================================
    // 第一部分：读取首个块寻找 TLV 头，获取数据长度
    // ==========================================
    uint8_t *keyToUse = GetKeyForBlock(currentAbsBlock);
    status = M1_ReadDataBlock_Abs(uid, currentAbsBlock, keyToUse, readBuf);
    if (status != MI_OK) return status;

    uint8_t tlvOffset = 0;
    while(tlvOffset < 16 && readBuf[tlvOffset] != 0x03) {
        if (readBuf[tlvOffset] == 0xFE) return MI_NOTAGERR; // 遇到结束符，卡是空的
        tlvOffset++;
    }
    
    if (tlvOffset >= 15) return MI_FORMATERR; 

    uint8_t ndefLen = readBuf[tlvOffset + 1];
    if (ndefLen == 0 || ndefLen > 100) return MI_FORMATERR; 

    #ifdef M1_AUTO_HALT_ENABLE
    uint8_t tagType[2];
    PcdRequest(PICC_REQALL, tagType);
    PcdAnticoll(uid);
    #endif

    // ==========================================
    // 第二部分：根据长度，将所需的所有块拉取到 buffer 内存中
    // ==========================================
    uint8_t buffer[NDEF_MAX_BUFFER_SIZE];
    uint8_t totalBytesToRead = tlvOffset + 2 + ndefLen + 1; 
    uint8_t blocksNeeded = (totalBytesToRead % 16 == 0) ? (totalBytesToRead / 16) : (totalBytesToRead / 16 + 1);
    
    for (uint8_t b = 0; b < blocksNeeded; b++) 
    {
        if (currentAbsBlock % 4 == 3) currentAbsBlock++; 

        keyToUse = GetKeyForBlock(currentAbsBlock);
        status = M1_ReadDataBlock_Abs(uid, currentAbsBlock, keyToUse, &buffer[b * 16]);
        if (status != MI_OK) return status;

        #ifdef M1_AUTO_HALT_ENABLE
        PcdRequest(PICC_REQALL, tagType);
        PcdAnticoll(uid);
        #endif

        currentAbsBlock++;
    }

    // ==========================================
    // 第三部分：循环解析 buffer 里的 NDEF 多记录
    // ==========================================
    uint8_t ndefStart = tlvOffset + 2; 
    uint8_t bytesParsed = 0;
    
    NDEF_Message_Init(msg); // 清空结构体准备解析

    // 循环解析直到读取完毕或达到最大支持记录数
    while (bytesParsed < ndefLen && msg->recordCount < NDEF_MAX_RECORDS) 
    {
        uint8_t headerByte = buffer[ndefStart + bytesParsed];
        uint8_t isME = (headerByte & 0x40) != 0; // 提取 ME 位检查是否是最后一条
        
        uint8_t typeLen = buffer[ndefStart + bytesParsed + 1];
        uint8_t payloadLen = buffer[ndefStart + bytesParsed + 2];
        uint8_t typeChar = buffer[ndefStart + bytesParsed + 3];
        
        NDEF_Record *rec = &msg->records[msg->recordCount];
        uint8_t payloadStart = ndefStart + bytesParsed + 3 + typeLen;
        
        if (typeChar == 'U') {
            rec->type = NDEF_TYPE_URI;
            rec->uriCode = buffer[payloadStart];
            rec->payloadLength = payloadLen - 1;
            memcpy(rec->payload, &buffer[payloadStart + 1], rec->payloadLength);
            msg->recordCount++;
        } 
        else if (typeChar == 'T') {
            rec->type = NDEF_TYPE_TEXT;
            uint8_t langLen = buffer[payloadStart] & 0x3F;
            memcpy(rec->language, &buffer[payloadStart + 1], langLen);
            rec->payloadLength = payloadLen - 1 - langLen;
            memcpy(rec->payload, &buffer[payloadStart + 1 + langLen], rec->payloadLength);
            msg->recordCount++;
        }
        
        // 偏移量推进到下一条记录 (SR模式下Header占用固定3字节：头+TypeLen+PayloadLen)
        bytesParsed += (3 + typeLen + payloadLen);
        
        // 如果当前记录是最后一条 (ME 位被置 1)，则停止解析
        if (isME) break; 
    }

    return MI_OK;
}

/**
 * @brief  [白卡格式化] 将出厂白卡初始化为标准 NDEF 卡片 (包含 MAD 目录)
 * @param  uid: 4字节卡号
 */
int8_t NDEF_FormatBlankCard(uint8_t *uid)
{
    int8_t status;
    uint8_t ndef_trailer[16];
    uint8_t mad_trailer[16];
    
    // 生成 NDEF 扇区的控制块 (KeyA=D3F7..., KeyB=FFFF...)
    Make_SectorTrailer(Key_NDEF, M1_DefaultKey, CTRL_FREE_DEV, ndef_trailer);
    // 生成 MAD 扇区的控制块 (KeyA=A0A1..., KeyB=FFFF...)
    Make_SectorTrailer(Key_MAD,  M1_DefaultKey, CTRL_FREE_DEV, mad_trailer);

    // 1. 写入 NFC Forum 规定的 MAD1 目录信息 (扇区 0)
    uint8_t mad_b1[16] = {0x14, 0x01, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
    uint8_t mad_b2[16] = {0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};

    M1_ForceWakeUp(uid); M1_WriteDataBlock_Abs(uid, 1, M1_DefaultKey, mad_b1);
    M1_ForceWakeUp(uid); M1_WriteDataBlock_Abs(uid, 2, M1_DefaultKey, mad_b2);
    
    // 修改扇区 0 密码为 Key_MAD
    M1_ForceWakeUp(uid); M1_WriteSectorControlBlock_Safe(uid, 0, M1_DefaultKey, mad_trailer);

    // 2. 遍历格式化扇区 1 到 15 (修改为 NDEF 密码)
    for (uint8_t sector = 1; sector < 16; sector++) 
    {
        M1_ForceWakeUp(uid);
        status = M1_WriteSectorControlBlock_Safe(uid, sector, M1_DefaultKey, ndef_trailer);
        if (status != MI_OK) return status;
    }

    // 3. 写入空的 NDEF 头部 (使用刚刚生效的新密码 Key_NDEF)
    uint8_t empty_ndef[16] = {0x03, 0x00, 0xFE, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    M1_ForceWakeUp(uid);
    status = M1_WriteDataBlock_Abs(uid, 4, Key_NDEF, empty_ndef);

    return status;
}

/**
 * @brief  [通用格式化] 自适应识别卡片状态并初始化
 * @param  uid: 4字节卡号
 * @return MI_OK(成功), MI_ACCESSERR(未知加密卡无法处理), MI_ERR(底层错误)
 */
int8_t NDEF_FormatUniversal(uint8_t *uid)
{
    int8_t status;
    uint8_t dummyBuf[16];

    // ==========================================
    // 阶段 1：试探是否已经是 NDEF 卡
    // ==========================================
    status = M1_ReadDataBlock_Abs(uid, 4, Key_NDEF, dummyBuf);
    if (status == MI_OK) 
    {
        // 密码正确，直接覆盖块 4 为空消息，清空旧数据
        uint8_t empty_ndef[16] = {0x03, 0x00, 0xFE, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        M1_ForceWakeUp(uid);
        return M1_WriteDataBlock_Abs(uid, 4, Key_NDEF, empty_ndef);
    }

    // ==========================================
    // 阶段 2：如果不是，试探是否是出厂白卡
    // ==========================================
    M1_ForceWakeUp(uid); // 试探失败卡片已休眠，必须强行唤醒
    
    status = M1_ReadDataBlock_Abs(uid, 4, M1_DefaultKey, dummyBuf);
    if (status != MI_OK) 
    {
        // 别人的加密卡，无法处理
        return MI_ACCESSERR; 
    }

    // ==========================================
    // 阶段 3：确认是出厂白卡,直接调用白卡格式化函数
    // ==========================================
    return NDEF_FormatBlankCard(uid);
}
