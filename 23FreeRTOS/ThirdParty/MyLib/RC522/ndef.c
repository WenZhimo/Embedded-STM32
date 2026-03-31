#include "ndef.h"
#include <stdio.h>

#define NDEF_MAX_BUFFER_SIZE 2048 
#define NDEF_START_BLOCK     4   

//  NDEF 格式化设计的控制位：自由读写，且 GPB 强制为 0x40 (MAD v1)
uint8_t CTRL_NDEF_FORMAT[4] = {0xFF, 0x07, 0x80, 0x40};

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
    if (ndefLen == 0 || ndefLen > NDEF_MAX_BUFFER_SIZE) return MI_FORMATERR; 

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
    Make_SectorTrailer(Key_NDEF, M1_DefaultKey, CTRL_NDEF_FORMAT, ndef_trailer);
    // 生成 MAD 扇区的控制块 (KeyA=A0A1..., KeyB=FFFF...)
    Make_SectorTrailer(Key_MAD,  M1_DefaultKey, CTRL_NDEF_FORMAT, mad_trailer);

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


/**
 * @brief  [恢复出厂设置] 将 NDEF 卡或其他已知密码的卡彻底恢复为出厂白卡状态
 * @param  uid: 4字节卡号
 * @return MI_OK(成功), MI_ACCESSERR(未知密码卡，无法恢复), MI_ERR(底层错误)
 */
int8_t NDEF_FormatToBlankCard(uint8_t *uid)
{
    int8_t status;
    uint8_t factory_trailer[16];
    uint8_t empty_data[16] = {0}; // 全 0 数据，用于抹除残留特征

    // 构造出厂默认的控制块：KeyA 和 KeyB 全是 0xFF，控制位为自由读写
    Make_SectorTrailer(M1_DefaultKey, M1_DefaultKey, CTRL_FREE_DEV, factory_trailer);

    // ==========================================
    // 遍历所有 16 个扇区，逐一“洗白”
    // ==========================================
    for (uint8_t sector = 0; sector < 16; sector++)
    {
        // 确定该扇区在 NDEF 标准下的预期密码
        uint8_t *expectedKey = (sector == 0) ? Key_MAD : Key_NDEF;

        // 1. 先尝试用出厂密码验证 (可能它本身就是没被改过的白卡扇区)
        M1_ForceWakeUp(uid);
        status = M1_WriteSectorControlBlock_Safe(uid, sector, M1_DefaultKey, factory_trailer);

        // 2. 如果出厂密码验证失败，说明密码被改过，尝试用预期的 NDEF 密码验证
        if (status != MI_OK)
        {
            M1_ForceWakeUp(uid); // 验证失败会导致卡片休眠，必须强行唤醒！
            status = M1_WriteSectorControlBlock_Safe(uid, sector, expectedKey, factory_trailer);

            // 3. 如果还是不行，说明这是一张未知密码的加密卡，直接中止操作并报错
            if (status != MI_OK) {
                return MI_ACCESSERR;
            }
        }

        // ==========================================
        // 清理残余特征数据 (使用刚刚恢复好的 M1_DefaultKey)
        // ==========================================
        if (sector == 0) 
        {
            // 抹除 MAD 目录
            M1_ForceWakeUp(uid); M1_WriteDataBlock_Abs(uid, 1, M1_DefaultKey, empty_data);
            M1_ForceWakeUp(uid); M1_WriteDataBlock_Abs(uid, 2, M1_DefaultKey, empty_data);
        }
        else if (sector == 1) 
        {
            // 抹除 NDEF 头部
            M1_ForceWakeUp(uid); M1_WriteDataBlock_Abs(uid, 4, M1_DefaultKey, empty_data);
        }
    }

    return MI_OK;
}


// 测试函数：向卡片写入一条 URI 记录 (用于验证写入功能)
void NDEF_Test_WriteToCard(uint8_t *uid){
    //uint8_t CardType[2];
    uint8_t CardUID[4];
    int8_t status;

    M1_ForceWakeUp(CardUID);

    // 格式化卡片为NDEF空白卡
    status=NDEF_FormatUniversal(CardUID);
    if (status != MI_OK) {
        printf("Failed to Format Card. Status Code: %d\r\n", status);
    }else {
        printf("Card Formatted Successfully!\r\n");
    }
    M1_ForceWakeUp(CardUID);
    
    NDEF_Message myNdefMsg;

    // 1. 初始化空消息
    NDEF_Message_Init(&myNdefMsg);
    

    // 2. 添加记录：让手机弹窗打开网页
    NDEF_AddUriRecord(&myNdefMsg, URI_PREFIX_HTTPS_WWWDOT, "wenzhimo.xyz");

    // 3. 添加记录：传递额外的文本信息
    NDEF_AddTextRecord(&myNdefMsg, "zh", "这卡片支持多条NDEF。");

    NDEF_AddTextRecord(&myNdefMsg, "zh", "如你所见，这些记录是在单片机上生成并写入的。");
    NDEF_AddTextRecord(&myNdefMsg, "zh", "想要更多记录？");
    NDEF_AddTextRecord(&myNdefMsg, "zh", "更多记录！");
    NDEF_AddTextRecord(&myNdefMsg, "zh", "更多！！！");

    // 4. 写入卡片 (会自动把上述两条记录打包并计算好头部 MB/ME 标志)
    status = NDEF_WriteToCard(CardUID, &myNdefMsg);
    
    if (status == MI_OK) {
        printf("NDEF Message Written Successfully!\r\n");
    } else {
        printf("Failed to Write NDEF Message. Status Code: %d\r\n", status);
    }

    // 操作完成后，让卡片休眠，防止一直重复读取
    PcdHalt(); 

}
// 测试函数：从卡片读取 NDEF 消息并打印内容 (用于验证读取功能)
void NDEF_Test_ReadFromCard(uint8_t *uid){
    //uint8_t CardType[2];
    uint8_t CardUID[4];
    // 为了稳定，读取前先统一唤醒一次，杜绝死锁
    M1_ForceWakeUp(CardUID);
    
    NDEF_Message readMsg;
    
    // 3. 从卡片读取 NDEF 消息
    int8_t status = NDEF_ReadFromCard(CardUID, &readMsg);
    
    if (status == MI_OK) {
        printf("NDEF Message Read Successfully!\r\n");
        printf(">>> 当前卡片包含 %d 条记录 <<<\r\n", readMsg.recordCount);
        printf("----------------------------------------\r\n");
        
        // 4. 遍历并解析打印每一条记录
        for (uint8_t i = 0; i < readMsg.recordCount; i++) {
            NDEF_Record *rec = &readMsg.records[i];
            printf("[记录 %d] ", i + 1);
            
            if (rec->type == NDEF_TYPE_URI) {
                // 还原 URI 的前缀
                const char *prefix = "";
                switch(rec->uriCode) {
                    case URI_PREFIX_HTTP_WWWDOT:  prefix = "http://www."; break;
                    case URI_PREFIX_HTTPS_WWWDOT: prefix = "https://www."; break;
                    case URI_PREFIX_HTTP:         prefix = "http://"; break;
                    case URI_PREFIX_HTTPS:        prefix = "https://"; break;
                    default:                      prefix = ""; break;
                }
                
                printf("类型: 网址 (URI)\r\n");
                // %.*s 的用法：先传长度(payloadLength)，再传字符串指针(payload)
                printf("          链接: %s%.*s\r\n", prefix, rec->payloadLength, rec->payload);
            } 
            else if (rec->type == NDEF_TYPE_TEXT) {
                printf("类型: 文本 (Text)\r\n");
                printf("          语言: %s\r\n", rec->language);
                printf("          内容: %.*s\r\n", rec->payloadLength, rec->payload);
            }
        }
        printf("----------------------------------------\r\n");
    } 
    else if (status == MI_NOTAGERR) {
        printf("读取完成：这是一张 NDEF 卡，但里面是空的（无记录）。\r\n");
    } 
    else {
        printf("Failed to Read NDEF Message. Status Code: %d\r\n", status);
    }

    // 5. 完美的退出三部曲，防止状态机死锁
    PcdHalt(); 
    PcdStopCrypto1(); 
}
