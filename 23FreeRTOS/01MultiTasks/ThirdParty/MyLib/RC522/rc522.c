#include "rc522.h"
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;

#define MAXRLEN 18

// 定义 M1 卡的出厂默认密钥 (A密钥，6个 0xFF)
uint8_t M1_DefaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// 定义两组 NDEF 标准密钥

uint8_t Key_MAD[6]  = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}; // 扇区0 (MAD目录) 密钥
uint8_t Key_NDEF[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}; // 扇区1~15 (NDEF区) 密钥

/* ================= M1卡控制位魔法字典 (Access Bits) ================= */
// 1. 自由开发模式：KeyA/B均可任意读写 (出厂默认)
uint8_t CTRL_FREE_DEV[4]=        {0xFF, 0x07, 0x80, 0x69};

// 2. 读写分离模式：KeyA只读，KeyB可读写且可修改密码
uint8_t CTRL_MASTER_SLAVE[4]=    {0x7F, 0x07, 0x88, 0x69};

// 3. 防篡改只读模式：KeyA/B均只读，仅KeyB可修改控制位，写时需要先用B修改控制位再写入数据
uint8_t CTRL_ANTI_TAMPER[4]=     {0x5F, 0xC7, 0x8A, 0x69};

// 4. 安全储值卡模式：KeyA仅支持查询/扣款，KeyB支持充值/初始化
uint8_t CTRL_SECURE_WALLET[4]=   {0x08, 0x77, 0x8F, 0x69};

/**
 * @brief  通过 SPI 读取 RC522 的寄存器
 * @param  Address: 要读取的寄存器地址
 * @return 寄存器中的当前值
 */
uint8_t ReadRawRC(uint8_t Address)
{
    uint8_t tx[2];
    uint8_t rx[2];

    // RC522 SPI 读地址格式：最高位(bit7)为1表示读，bit6~bit1为实际地址，最低位(bit0)为0
    tx[0] = ((Address << 1) & 0x7E) | 0x80; 
    tx[1] = 0; // 发送一个空字节（dummy byte）以产生时钟信号，从而接收数据

    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET); // 拉低片选，开始通信
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);           // 全双工收发
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);   // 拉高片选，结束通信

    return rx[1]; // 返回接收到的第二个字节（第一个字节是状态字，通常丢弃）
}

/**
 * @brief  通过 SPI 向 RC522 的寄存器写入数据
 * @param  Address: 要写入的寄存器地址
 * @param  value: 要写入的数据值
 */
void WriteRawRC(uint8_t Address, uint8_t value)
{
    uint8_t tx[2];

    // RC522 SPI 写地址格式：最高位(bit7)为0表示写，bit6~bit1为实际地址，最低位(bit0)为0
    tx[0] = ((Address << 1) & 0x7E);
    tx[1] = value; // 紧接着发送要写入的数据

    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY); // 仅发送数据
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief  置位 RC522 寄存器的特定位 (按位或)
 * @param  reg: 寄存器地址
 * @param  mask: 掩码，为 1 的位将被置位
 */
void SetBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t tmp = ReadRawRC(reg);
    WriteRawRC(reg, tmp | mask);
}

/**
 * @brief  清零 RC522 寄存器的特定位 (按位与非)
 * @param  reg: 寄存器地址
 * @param  mask: 掩码，为 1 的位将被清零
 */
void ClearBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t tmp = ReadRawRC(reg);
    WriteRawRC(reg, tmp & (~mask));
}

/**
 * @brief  RC522 硬件与软件复位及初始化 (使用 HAL_Delay)
 * @note   适用于裸机环境或任务调度器启动前
 * @return MI_OK 表示成功
 */
int8_t PcdReset(void)
{
    // 1. 硬件复位：通过 RST 引脚产生复位脉冲
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    #ifndef RF_USE_RTOS
    HAL_Delay(2);
    #else
    osDelay(2);
    #endif
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET);
    #ifndef RF_USE_RTOS
    HAL_Delay(2);
    #else
    osDelay(2);
    #endif
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    #ifndef RF_USE_RTOS
    HAL_Delay(2);
    #else
    osDelay(2);
    #endif

    // 2. 软件复位：向命令寄存器写入复位命令
    WriteRawRC(CommandReg, PCD_RESETPHASE);
    
    // 必须等待 RC522 内部软复位完成！否则后面的配置全被丢弃！
    #ifndef RF_USE_RTOS
    HAL_Delay(50);
    #else
    osDelay(50);
    #endif

    // 3. 配置内部定时器和工作模式 (定义底层通信速率与超时机制)
    WriteRawRC(ModeReg, 0x3D);       // 定义发送和接收常用模式
    WriteRawRC(TReloadRegL, 30);     // 定时器重装值低位
    WriteRawRC(TReloadRegH, 0);      // 定时器重装值高位
    WriteRawRC(TModeReg, 0x8D);      // 内部定时器设置
    WriteRawRC(TPrescalerReg, 0x3E); // 设置定时器分频系数

    // 【关键修复2】强制 100% ASK 调制！没有这句 M1 卡绝对不理你
    WriteRawRC(TxAutoReg, 0x40); 
    
    // 可选增强：将天线接收增益调到最大 (48dB)，解决稍微离远一点就读不到的问题
    // WriteRawRC(RxSelReg, 0x86); 

    return MI_OK;
}





/**
 * @brief  开启天线发射
 * @note   每次与卡片通信前必须确保天线已开启
 */
void PcdAntennaOn(void)
{
    uint8_t temp = ReadRawRC(TxControlReg);
    // 检查天线引脚 TX1 和 TX2 的控制位是否已开启
    if(!(temp & 0x03))
    {
        SetBitMask(TxControlReg, 0x03);
    }
}

/**
 * @brief  关闭天线发射
 */
void PcdAntennaOff(void)
{
    ClearBitMask(TxControlReg, 0x03);
}

/**
 * @brief  利用 RC522 的硬件协处理器计算 CRC 校验码
 * @param  pIndata: 需要计算 CRC 的数据首地址
 * @param  len: 数据长度
 * @param  pOutData: 存放计算结果的 2 字节数组
 */
void CalulateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData)
{
    uint8_t i, n;

    // 清除 CRC 中断标志，停止当前活动，清空 FIFO
    ClearBitMask(DivIrqReg, 0x04);
    WriteRawRC(CommandReg, PCD_IDLE);
    SetBitMask(FIFOLevelReg, 0x80); // 写 1 清空 FIFO

    // 将需要计算的数据压入 FIFO
    for(i = 0; i < len; i++)
    {
        WriteRawRC(FIFODataReg, pIndata[i]);
    }

    // 触发硬件 CRC 计算命令
    WriteRawRC(CommandReg, PCD_CALCCRC);

    i = 0xFF;
    // 轮询等待 CRC 计算完成标志 (位 2)
    do
    {
        n = ReadRawRC(DivIrqReg);
        i--;
    }
    while((i != 0) && !(n & 0x04));

    // 读出 2 字节的 CRC 计算结果
    pOutData[0] = ReadRawRC(CRCResultRegL);
    pOutData[1] = ReadRawRC(CRCResultRegM);
}

/**
 * @brief  RC522 与 ISO14443 卡片通信的核心底层函数 (状态机)
 * @param  Command:   发送给 RC522 的命令字 (如发送、认证等)
 * @param  pInData:   准备发送给卡片的数据
 * @param  InLenByte: 发送数据的字节长度
 * @param  pOutData:  用于接收卡片返回数据的缓冲区
 * @param  pOutLenBit: 接收到的数据长度 (以 bit 为单位)
 * @return 状态码 (MI_OK, MI_NOTAGERR, MI_ERR)
 */
int8_t PcdComMF522(uint8_t Command, uint8_t *pInData, uint8_t InLenByte, uint8_t *pOutData, uint16_t *pOutLenBit)
{
    int8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitFor = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    // 1. 根据不同命令配置期望发生的中断类型
    switch(Command)
    {
        case PCD_AUTHENT:     // 密钥认证
            irqEn = 0x12;     // 允许错误中断 | 空闲中断
            waitFor = 0x10;   // 等待空闲中断 (认证完成)
            break;

        case PCD_TRANSCEIVE:  // 收发数据
            irqEn = 0x77;     // 允许 Tx/Rx/Idle/Err 等全部中断
            waitFor = 0x30;   // 等待接收中断 | 空闲中断
            break;
    }

    // 2. 初始化中断及 FIFO 状态
    WriteRawRC(ComIEnReg, irqEn | 0x80); // 使能中断请求
    ClearBitMask(ComIrqReg, 0x80);       // 清除所有中断标志
    WriteRawRC(CommandReg, PCD_IDLE);    // 停止正在进行的命令
    SetBitMask(FIFOLevelReg, 0x80);      // 清空 FIFO 指针

    // 3. 将发送数据压入 FIFO
    for(i = 0; i < InLenByte; i++)
    {
        WriteRawRC(FIFODataReg, pInData[i]);
    }

    // 4. 执行命令
    WriteRawRC(CommandReg, Command);

    // 如果是收发命令，需要设置 BitFramingReg 发送触发位 (启动数据发送)
    if(Command == PCD_TRANSCEIVE)
        SetBitMask(BitFramingReg, 0x80);

    // 5. 等待通信完成或超时 (使用 HAL_GetTick 实现非阻塞延时)
    uint32_t start = HAL_GetTick();
    do
    {
        n = ReadRawRC(ComIrqReg);
    }
    // 循环条件：定时器未归零(n&0x01) && 未收到预期中断(n&waitFor) && 未超时(30ms)
    while(!(n & 0x01) && !(n & waitFor) && ((HAL_GetTick() - start) < 30));

    ClearBitMask(BitFramingReg, 0x80); // 发送结束，清除触发位

    // 6. 检查执行结果
    if(!(ReadRawRC(ErrorReg) & 0x1B))  // 检查是否有底层协议错误 (BufferOvfl, CollErr, ParityErr, ProtocolErr)
    {
        status = MI_OK;

        if(n & irqEn & 0x01) // 如果发生了定时器中断，说明没读到卡
            status = MI_NOTAGERR;

        if(Command == PCD_TRANSCEIVE)
        {
            // 获取 FIFO 中接收到的字节数
            n = ReadRawRC(FIFOLevelReg);
            // 检查最后一个字节的有效位数
            lastBits = ReadRawRC(ControlReg) & 0x07;

            // 计算接收到的总位数 (Bits)
            if(lastBits)
                *pOutLenBit = (n - 1) * 8 + lastBits;
            else
                *pOutLenBit = n * 8;

            // 异常数据长度保护
            if(n == 0) n = 1;
            if(n > MAXRLEN) n = MAXRLEN;

            // 从 FIFO 中提取接收到的数据
            for(i = 0; i < n; i++)
                pOutData[i] = ReadRawRC(FIFODataReg);
        }
    }

    SetBitMask(ControlReg, 0x80);     // 停止定时器
    WriteRawRC(CommandReg, PCD_IDLE); // 将芯片置为空闲

    return status;
}

/**
 * @brief  寻卡：寻找天线工作区内的卡片
 * @param  req_code: 寻卡方式 (PICC_REQIDL:寻找休眠外卡, PICC_REQALL:寻找所有卡)
 * @param  pTagType: 返回 2 字节的卡片类型代码 (如 0x0400 为 M1 S50 卡)
 * @return MI_OK 表示寻卡成功
 */
int8_t PcdRequest(uint8_t req_code, uint8_t *pTagType)
{
    int8_t status;
    uint16_t len;
    uint8_t buf[MAXRLEN];

    ClearBitMask(Status2Reg, 0x08); // 关闭加密体制 (MIFARE Crypto1)
    WriteRawRC(BitFramingReg, 0x07); // 发送寻卡指令时只需发送 7 bit
    SetBitMask(TxControlReg, 0x03);  // 确保天线开启

    buf[0] = req_code;

    // 与卡片收发寻卡指令
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 1, buf, &len);

    // 寻卡成功时，卡片应答 (ATQA) 长度为 16 bits (0x10)
    if(status == MI_OK && len == 0x10)
    {
        pTagType[0] = buf[0];
        pTagType[1] = buf[1];
    }
    else
        status = MI_ERR;

    return status;
}

/**
 * @brief  防冲突：检测工作区内的所有卡，并获取其中一张卡的 UID (序列号)
 * @param  pSnr: 用于存放返回的 4 字节卡号 (UID)
 * @return MI_OK 表示成功获取 UID
 */
int8_t PcdAnticoll(uint8_t *pSnr)
{
    int8_t status;
    uint8_t i, snr_check = 0;
    uint16_t len;
    uint8_t buf[MAXRLEN];

    ClearBitMask(Status2Reg, 0x08);
    WriteRawRC(BitFramingReg, 0x00); // 恢复 8 bit 完整帧发送
    ClearBitMask(CollReg, 0x80);     // 开启防冲突机制

    buf[0] = PICC_ANTICOLL1;         // 防冲突命令 0x93
    buf[1] = 0x20;                   // 级联级别 1，发送 2 个字节

    status = PcdComMF522(PCD_TRANSCEIVE, buf, 2, buf, &len);

    if(status == MI_OK)
    {
        // 提取 4 字节 UID 并进行异或校验
        for(i = 0; i < 4; i++)
        {
            pSnr[i] = buf[i];
            snr_check ^= buf[i]; // 第 5 个字节是前 4 个字节的异或和 (BCC)
        }

        if(snr_check != buf[4])
            status = MI_ERR; // 校验失败说明可能存在多卡冲突
    }

    SetBitMask(CollReg, 0x80); // 关闭防冲突

    return status;
}

/**
 * @brief  选卡：将 RC522 的焦点锁定在指定 UID 的卡片上
 * @param  pSnr: 4 字节的卡号 (由 PcdAnticoll 获取)
 * @return MI_OK 表示选卡成功
 */
int8_t PcdSelect(uint8_t *pSnr)
{
    int8_t status;
    uint8_t i;
    uint16_t unLen;
    uint8_t buf[MAXRLEN]; 
    
    // 构造选卡帧：指令 + 长度 + 4字节UID + 1字节BCC校验 + 2字节CRC
    buf[0] = PICC_ANTICOLL1;
    buf[1] = 0x70; // 0x70 表示发送 7 个字节
    buf[6] = 0;
    for (i = 0; i < 4; i++)
    {
    	buf[i+2] = pSnr[i];
    	buf[6]  ^= pSnr[i]; // 重新计算 BCC 放入 buf[6]
    }
    CalulateCRC(buf, 7, &buf[7]); // 追加 CRC 校验码到末尾
  
    ClearBitMask(Status2Reg, 0x08);

    status = PcdComMF522(PCD_TRANSCEIVE, buf, 9, buf, &unLen);
    
    // 选卡成功后，卡片会回复 SAK(Select Acknowledge)，长度为 24 bit (0x18) 或 8 bit，这里严格校验为 0x18
    if ((status == MI_OK) && (unLen == 0x18))
    {   status = MI_OK;  }
    else
    {   status = MI_ERR; }

    return status;
}

/**
 * @brief  验证扇区密码：通过三步相互认证建立加密通信管道
 * @param  auth_mode: 认证模式 (PICC_AUTHENT1A 或 PICC_AUTHENT1B)
 * @param  addr: 需要访问的块地址 (0~63)
 * @param  pKey: 6字节的密钥数组
 * @param  pSnr: 当前操作卡片的 4字节 UID
 * @return MI_OK 表示认证成功
 */
int8_t PcdAuthState(uint8_t auth_mode, uint8_t addr, uint8_t *pKey, uint8_t *pSnr)
{
    int8_t status;
    uint16_t len;
    uint8_t i, buf[MAXRLEN];

    // 构造认证帧：认证命令 + 块地址 + 6字节密钥 + 4字节UID
    buf[0] = auth_mode;
    buf[1] = addr;

    for(i = 0; i < 6; i++)
        buf[i+2] = pKey[i];

    for(i = 0; i < 4; i++)   // 修复：UID只有4字节
        buf[i+8] = pSnr[i];

    status = PcdComMF522(PCD_AUTHENT, buf, 12, buf, &len);

    // 认证成功后，RC522 内部的 Crypto1 单元会自动置位 Status2Reg 的 Crypto1On 位(bit3)
    if(status != MI_OK || !(ReadRawRC(Status2Reg) & 0x08))
        status = MI_ERR;

    return status;
}

/**
 * @brief  读取卡片块数据：从已认证的块读取 16 字节
 * @param  addr: 要读取的块地址
 * @param  pData: 用于存放读取结果的缓冲区 (至少16字节)
 * @return MI_OK 表示读取成功
 */
int8_t PcdRead(uint8_t addr, uint8_t *pData)
{
    int8_t status;
    uint16_t unLen;
    uint8_t i, buf[MAXRLEN]; 

    // 构造读块帧：读指令 + 块地址 + 2字节CRC
    buf[0] = PICC_READ;
    buf[1] = addr;
    CalulateCRC(buf, 2, &buf[2]);
   
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 4, buf, &unLen);
    
    // M1卡读块成功将返回 16 字节数据 + 2 字节 CRC，总计 18 字节 (144 bits = 0x90)
    if ((status == MI_OK) && (unLen == 0x90))
    {
        for (i = 0; i < 16; i++)
        {    pData[i] = buf[i];   }
    }
    else
    {   status = MI_ERR;   }
    
    return status;
}

/**
 * @brief  写入卡片块数据：向已认证的块写入 16 字节
 * @param  addr: 要写入的块地址
 * @param  pData: 准备写入的 16 字节数据源
 * @return MI_OK 表示写入成功
 */
int8_t PcdWrite(uint8_t addr, uint8_t *pData)
{
    int8_t status;
    uint16_t unLen;
    uint8_t i, buf[MAXRLEN]; 
    
    // M1卡写操作分为两步：
    // Step 1: 发送写块指令
    buf[0] = PICC_WRITE;
    buf[1] = addr;
    CalulateCRC(buf, 2, &buf[2]);
 
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 4, buf, &unLen);

    // 卡片应答 4 bits，且包含 0x0A 表示 ACK (准备好接收数据)
    if ((status != MI_OK) || (unLen != 4) || ((buf[0] & 0x0F) != 0x0A))
    {   status = MI_ERR;   }
        
    // Step 2: 发送要写入的具体 16 字节数据
    if (status == MI_OK)
    {
        for (i = 0; i < 16; i++)
        {    buf[i] = pData[i];   }
        CalulateCRC(buf, 16, &buf[16]); // 追加 CRC

        status = PcdComMF522(PCD_TRANSCEIVE, buf, 18, buf, &unLen);
        
        // 卡片再次应答 ACK (0x0A) 表示写入物理存储区成功
        if ((status != MI_OK) || (unLen != 4) || ((buf[0] & 0x0F) != 0x0A))
        {   status = MI_ERR;   }
    }
    
    return status;
}

/**
 * @brief  使目标卡片进入休眠状态
 * @note   通信结束后必须调用，释放卡片，否则卡片停留在当前状态不会响应下一次寻卡
 * @return 状态码 (通常返回错误是正常的，因为卡片休眠后不再应答)
 */
int8_t PcdHalt(void)
{
    int8_t status;
    uint16_t len;
    uint8_t buf[MAXRLEN];

    buf[0] = PICC_HALT;
    buf[1] = 0;

    CalulateCRC(buf, 2, &buf[2]);

    status = PcdComMF522(PCD_TRANSCEIVE, buf, 4, buf, &len);

    return status;
}

/**
 * @brief  【高层封装】向 M1 卡的指定块写入 16 字节数据
 * @param  uid: 4字节卡号 (由 PcdAnticoll 获取)
 * @param  blockAddr: 要写入的绝对块号 (0~63)
 * @param  key: 6字节扇区密钥 (通常是 A 密钥)
 * @param  writeData: 准备写入的 16 字节数据数组
 * @return MI_OK (0) 表示成功，其他值表示失败
 */
int8_t M1_WriteCardBlock(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *writeData)
{
    int8_t status;
    #ifndef M1_AUTO_HALT_ENABLE
    static uint8_t lastUid[4] = {0};
    // 1. 选卡：与目标卡片建立专属通信
    // 检查是否是新卡
    if (memcmp(uid, lastUid, 4) != 0)
    {
        memcpy(lastUid, uid, 4); // 更新 lastUid
    }// 不是新卡，说明之前已经选过了，可以跳过选卡步骤直接认证
    else{
    #endif
        status = PcdSelect(uid);
        if (status != MI_OK) return status;
    #ifndef M1_AUTO_HALT_ENABLE
    }
    #endif
    

    // 2. 验证密钥：使用 A 密钥 (PICC_AUTHENT1A) 验证指定块所在的扇区
    status = PcdAuthState(PICC_AUTHENT1A, blockAddr, key, uid);
    if (status != MI_OK) return status;

    // 3. 写入数据：向指定块安全写入 16 字节
    status = PcdWrite(blockAddr, writeData);
    if (status != MI_OK) return status;

    PcdStopCrypto1(); // 断开加密通信管道，防止后续操作受干扰
    #ifdef M1_AUTO_HALT_ENABLE
    // 4. 休眠卡片：重置状态，等待下次寻卡
    PcdHalt();
    #endif

    return MI_OK;
}

/**
 * @brief  【高层封装】从 M1 卡的指定块读取 16 字节数据
 * @param  uid: 4字节卡号 (由 PcdAnticoll 获取)
 * @param  blockAddr: 要读取的绝对块号 (0~63)
 * @param  key: 6字节扇区密钥 (通常是 A 密钥)
 * @param  readData: 用于存放读取数据的 16 字节数组
 * @return MI_OK (0) 表示成功，其他值表示失败
 */
int8_t M1_ReadCardBlock(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *readData)
{
    int8_t status;

    // 1. 选卡
    status = PcdSelect(uid);
    if (status != MI_OK) return status;

    // 2. 验证扇区密钥
    status = PcdAuthState(PICC_AUTHENT1A, blockAddr, key, uid);
    if (status != MI_OK) return status;

    // 3. 执行读取
    status = PcdRead(blockAddr, readData);
    if (status != MI_OK) return status;

    PcdStopCrypto1(); // 断开加密通信管道，防止后续操作受干扰
    #ifdef M1_AUTO_HALT_ENABLE
    // 4. 休眠卡片
    PcdHalt();
    #endif

    return MI_OK;
}


/**
 * @brief   从 M1 卡读取 16 字节，支持选择验证 Key A 还是 Key B
 * @param  auth_mode: 验证模式 (填入 PICC_AUTHENT1A 或 PICC_AUTHENT1B)
 */
int8_t M1_ReadCardBlock_Ext(uint8_t *uid, uint8_t blockAddr, uint8_t auth_mode, uint8_t *key, uint8_t *readData)
{
    int8_t status;

    status = PcdSelect(uid);
    if (status != MI_OK) return status;

    // 动态传入验证模式，不再写死 Key A！
    status = PcdAuthState(auth_mode, blockAddr, key, uid);
    if (status != MI_OK) return status;

    status = PcdRead(blockAddr, readData);
    if (status != MI_OK) return status;

    PcdHalt();
    return MI_OK;
}

/**
 * @brief  向 M1 卡写入 16 字节，支持选择验证 Key A 还是 Key B
 */
int8_t M1_WriteCardBlock_Ext(uint8_t *uid, uint8_t blockAddr, uint8_t auth_mode, uint8_t *key, uint8_t *writeData)
{
    int8_t status;

    status = PcdSelect(uid);
    if (status != MI_OK) return status;

    // 动态传入验证模式
    status = PcdAuthState(auth_mode, blockAddr, key, uid);
    if (status != MI_OK) return status;

    status = PcdWrite(blockAddr, writeData);
    if (status != MI_OK) return status;

    PcdHalt();
    return MI_OK;
}



/**
 * @brief  [安全写] 向指定绝对块地址写入数据，严格拒绝写控制块
 * @param  uid: 4字节卡号
 * @param  blockAddr: 绝对块号 (0~63)
 * @param  key: 扇区密钥
 * @param  writeData: 16字节数据
 * @return MI_OK (成功), MI_ACCESSERR (拒绝访问) 或其他错误码
 */
int8_t M1_WriteDataBlock_Abs(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *writeData)
{
    // 拦截：块0是厂商固化只读块，写不进去
    if (blockAddr == 0) return MI_ACCESSERR;
    
    // 拦截：如果块号除以4余3，说明它是控制块，拒绝写入业务数据
    if ((blockAddr % 4) == 3) return MI_ACCESSERR;

    if (blockAddr > 63 ) return MI_ERR; // 越界保护

    return M1_WriteCardBlock(uid, blockAddr, key, writeData);
}

/**
 * @brief  [安全读] 从指定绝对块地址读取数据
 */
int8_t M1_ReadDataBlock_Abs(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *readData)
{
    // 拦截：控制块的 Key A 是无法读出的(读出全为0)，业务逻辑通常也不需要读出访问控制位
    //if ((blockAddr % 4) == 3) return MI_ACCESSERR;

    if (blockAddr > 63 ) return MI_ERR; // 越界保护

    return M1_ReadCardBlock(uid, blockAddr, key, readData);
}



/**
 * @brief  [相对安全写] 指定扇区号和相对数据块号写入数据
 * @param  sector: 扇区号 (0~15)
 * @param  relBlock: 相对数据块号 (仅限 0, 1, 2)
 */
int8_t M1_WriteDataBlock_Rel(uint8_t *uid, uint8_t sector, uint8_t relBlock, uint8_t *key, uint8_t *writeData)
{
    // 拦截：限制只能操作每个扇区的前3个数据块
    if (relBlock >= 3) return MI_ACCESSERR; 
    
    // 拦截：扇区 0 的相对块 0 是厂商 UID 块，不可写
    if (sector == 0 && relBlock == 0) return MI_ACCESSERR;

    if (sector > 15) return MI_ERR; // 越界保护
    
    // 换算绝对块号：扇区号 * 4 + 相对块号
    uint8_t absBlock = (sector * 4) + relBlock;

    return M1_WriteCardBlock(uid, absBlock, key, writeData);
}

/**
 * @brief  [相对读] 指定扇区号和相对数据块号读取数据
 */
int8_t M1_ReadDataBlock_Rel(uint8_t *uid, uint8_t sector, uint8_t relBlock, uint8_t *key, uint8_t *readData)
{
    // 拦截：限制读取范围
    //if (relBlock >= 3) return MI_ACCESSERR; 


    if (sector > 15) return MI_ERR; // 越界保护
    
    // 换算绝对块号：扇区号 * 4 + 相对块号
    uint8_t absBlock = (sector * 4) + relBlock;

    return M1_ReadCardBlock(uid, absBlock, key, readData);
}


/**
 * @brief  [高危专属写] 修改指定扇区的密码和访问控制位，带硬核防砖校验
 * @param  uid: 4字节卡号
 * @param  sector: 要修改的扇区号 (0~15)
 * @param  key: 当前验证该扇区所使用的合法密钥
 * @param  newControlData: 严格构造的16字节控制块数据 
 * (格式: 6字节新KeyA + 4字节控制位 + 6字节新KeyB)
 * @return MI_OK(成功), MI_ACCESSERR(越界), MI_FORMATERR(控制位校验失败杜绝变砖)
 */
int8_t M1_WriteSectorControlBlock_Safe(uint8_t *uid, uint8_t sector, uint8_t *key, uint8_t *newControlData)
{
    // 1. 扇区范围保护
    if (sector > 15) return MI_ACCESSERR;

    // 2. 核心防砖机制：校验访问控制位 (数组的第 6, 7, 8 字节)
    uint8_t b6 = newControlData[6];
    uint8_t b7 = newControlData[7];
    uint8_t b8 = newControlData[8];

    // 提取反码 (~C1, ~C2, ~C3)
    uint8_t inv_c1 = b6 & 0x0F;               // b6 的低 4 位
    uint8_t inv_c2 = (b6 >> 4) & 0x0F;        // b6 的高 4 位
    uint8_t inv_c3 = b7 & 0x0F;               // b7 的低 4 位

    // 提取原码 (C1, C2, C3)
    uint8_t c1 = (b7 >> 4) & 0x0F;            // b7 的高 4 位
    uint8_t c2 = b8 & 0x0F;                   // b8 的低 4 位
    uint8_t c3 = (b8 >> 4) & 0x0F;            // b8 的高 4 位

    // 严苛比对：原码与反码必须严格按位取反后相等 (即与 0x0F 异或后相等)
    if ( inv_c1 != (c1 ^ 0x0F) ||
         inv_c2 != (c2 ^ 0x0F) ||
         inv_c3 != (c3 ^ 0x0F) )
    {
        // 发现控制位格式异常！坚决拦截，返回格式错误
        return MI_FORMATERR; 
    }

    // 3. 强制把目标锁定在当前扇区的控制块 (相对块3)
    uint8_t targetAbsBlock = (sector * 4) + 3;

    // 4. 执行底层物理写入
    return M1_WriteCardBlock(uid, targetAbsBlock, key, newControlData);
}

/**
 * @brief  构建 M1 卡控制块数据 (16字节)
 * @param  newKeyA: 6字节的新 Key A
 * @param  newKeyB: 6字节的新 Key B (如果不打算用 Key B，传全 0xFF 的数组)
 * @param  ctrlBits: 4字节的控制位与GPB组合 
 *          (例如: CTRL_FREE_DEV、CTRL_MASTER_SLAVE、CTRL_ANTI_TAMPER、CTRL_SECURE_WALLET 等，
 *           或直接传入四字节数组)
 * @param  outTrailer: 输出参数，存入生成好的 16 字节控制块数据
 */
void Make_SectorTrailer(uint8_t *newKeyA, uint8_t *newKeyB, uint8_t *ctrlBits, uint8_t *outTrailer)
{
    uint8_t i;

    // 1. 填入前 6 字节：Key A
    for(i = 0; i < 6; i++) {
        outTrailer[i] = newKeyA[i];
    }

    // 2. 填入中间 4 字节：Access Bits + GPB
    for(i = 0; i < 4; i++) {
        outTrailer[6 + i] = ctrlBits[i];
    }

    // 3. 填入后 6 字节：Key B
    for(i = 0; i < 6; i++) {
        outTrailer[10 + i] = newKeyB[i];
    }
}

/**
 * @brief  主动关闭 RC522 的 Crypto1 硬件加密引擎
 * @note   在任何一次密码验证成功并读写完毕后，强烈建议调用此函数释放状态机
 */
void PcdStopCrypto1(void)
{
    // 清除 Status2Reg 的 bit3 (Crypto1On)
    ClearBitMask(Status2Reg, 0x08);
}



/**
 * @brief  [唤醒] 复位卡片与 RC522 状态机，确保回到 Ready 状态
 * @param  uid: 用于接收唤醒后重新获取的 4 字节卡号
 * @return MI_OK (0) 表示卡片已完全就绪，其他值表示感应区确实无卡
 * @note   该函数会先尝试温柔唤醒（寻卡），如果失败则通过断电重启卡片来强制复位状态机，最后再次寻卡并获取 UID
 */
int8_t M1_ForceWakeUp(uint8_t *uid)
{
    int8_t status;
    uint8_t tagType[2];

    // 1. 【RC522 侧复位】强制关闭硬件加密机，解除读卡器端的 Crypto1 死锁
    ClearBitMask(Status2Reg, 0x08); 

    // 2. 尝试温柔唤醒 (寻呼所有卡片，包括休眠卡)
    status = PcdRequest(PICC_REQALL, tagType);

    // 3. 【卡片侧物理复位】如果卡片不理人，断电重启
    if (status != MI_OK)
    {
        PcdAntennaOff(); // 关闭天线
        
        // 延时让卡片释放残余电量并复位
        #ifdef RF_USE_RTOS
            osDelay(5);
        #else
            HAL_Delay(5);
        #endif
        
        PcdAntennaOn();  // 重新开启天线给卡片供电
        
        // 等待卡片内部系统上电启动完毕
        #ifdef RF_USE_RTOS
            osDelay(5);
        #else
            HAL_Delay(5);
        #endif

        // 再次强制清除一下加密位，防止天线操作带来抖动
        ClearBitMask(Status2Reg, 0x08);

        // 重新尝试寻卡
        status = PcdRequest(PICC_REQALL, tagType);
        
        // 如果物理断电重启后还是寻不到卡，说明感应区真的没卡，或者卡拿远了
        if (status != MI_OK) 
        {
            return MI_NOTAGERR; 
        }
    }

    // 4. 防冲突：获取卡片唯一的 UID
    status = PcdAnticoll(uid);
    if (status != MI_OK) return status;

    
    return status;
}