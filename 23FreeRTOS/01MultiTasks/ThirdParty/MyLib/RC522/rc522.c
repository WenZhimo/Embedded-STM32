#include "rc522.h"
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;

#define MAXRLEN 18


// 定义 M1 卡的出厂默认密钥 (A密钥，6个 0xFF)
uint8_t M1_DefaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


 uint8_t ReadRawRC(uint8_t Address)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = ((Address << 1) & 0x7E) | 0x80;
    tx[1] = 0;

    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);

    return rx[1];
}

 void WriteRawRC(uint8_t Address,uint8_t value)
{
    uint8_t tx[2];

    tx[0] = ((Address << 1) & 0x7E);
    tx[1] = value;

    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);
}

 void SetBitMask(uint8_t reg,uint8_t mask)
{
    uint8_t tmp = ReadRawRC(reg);
    WriteRawRC(reg,tmp | mask);
}

 void ClearBitMask(uint8_t reg,uint8_t mask)
{
    uint8_t tmp = ReadRawRC(reg);
    WriteRawRC(reg,tmp & (~mask));
}

int8_t PcdReset(void)
{
    // 1. 硬件复位
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(2); // HAL 中推荐使用 HAL_Delay
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(2);

    // 2. 软件复位
    WriteRawRC(CommandReg, PCD_RESETPHASE);
    
    // 【关键修复1】必须等待 RC522 内部软复位完成！否则后面的配置全被丢弃！
    HAL_Delay(50); 

    // 3. 配置内部定时器和工作模式
    WriteRawRC(ModeReg, 0x3D);
    WriteRawRC(TReloadRegL, 30);
    WriteRawRC(TReloadRegH, 0);
    WriteRawRC(TModeReg, 0x8D);
    WriteRawRC(TPrescalerReg, 0x3E);

    // 【关键修复2】强制 100% ASK 调制！没有这句 M1 卡绝对不理你
    WriteRawRC(TxAutoReg, 0x40); 
    
    // 可选增强：将天线接收增益调到最大 (48dB)，解决稍微离远一点就读不到的问题
    // WriteRawRC(RxSelReg, 0x86); 

    return MI_OK;
}

int8_t PcdReset_rtos(void)
{
    // 1. 硬件复位
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    osDelay(2); // FreeRTOS 中推荐使用 osDelay
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET);
    osDelay(2);
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    osDelay(2);

    // 2. 软件复位
    WriteRawRC(CommandReg, PCD_RESETPHASE);
    
    // 【关键修复1】必须等待 RC522 内部软复位完成！否则后面的配置全被丢弃！
    osDelay(50); 

    // 3. 配置内部定时器和工作模式
    WriteRawRC(ModeReg, 0x3D);
    WriteRawRC(TReloadRegL, 30);
    WriteRawRC(TReloadRegH, 0);
    WriteRawRC(TModeReg, 0x8D);
    WriteRawRC(TPrescalerReg, 0x3E);

    // 【关键修复2】强制 100% ASK 调制！没有这句 M1 卡绝对不理你
    WriteRawRC(TxAutoReg, 0x40); 
    
    // 可选增强：将天线接收增益调到最大 (48dB)，解决稍微离远一点就读不到的问题
    // WriteRawRC(RxSelReg, 0x86); 

    return MI_OK;
}

void PcdAntennaOn(void)
{
    uint8_t temp = ReadRawRC(TxControlReg);

    if(!(temp & 0x03))
    {
        SetBitMask(TxControlReg,0x03);
    }
}

void PcdAntennaOff(void)
{
    ClearBitMask(TxControlReg,0x03);
}

 void CalulateCRC(uint8_t *pIndata,uint8_t len,uint8_t *pOutData)
{
    uint8_t i,n;

    ClearBitMask(DivIrqReg,0x04);
    WriteRawRC(CommandReg,PCD_IDLE);
    SetBitMask(FIFOLevelReg,0x80);

    for(i=0;i<len;i++)
    {
        WriteRawRC(FIFODataReg,pIndata[i]);
    }

    WriteRawRC(CommandReg,PCD_CALCCRC);

    i=0xFF;

    do
    {
        n=ReadRawRC(DivIrqReg);
        i--;
    }
    while((i!=0) && !(n&0x04));

    pOutData[0]=ReadRawRC(CRCResultRegL);
    pOutData[1]=ReadRawRC(CRCResultRegM);
}

 int8_t PcdComMF522(uint8_t Command,
                          uint8_t *pInData,
                          uint8_t InLenByte,
                          uint8_t *pOutData,
                          uint16_t *pOutLenBit)
{
    int8_t status = MI_ERR;
    uint8_t irqEn=0x00;
    uint8_t waitFor=0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    switch(Command)
    {
        case PCD_AUTHENT:
            irqEn=0x12;
            waitFor=0x10;
            break;

        case PCD_TRANSCEIVE:
            irqEn=0x77;
            waitFor=0x30;
            break;
    }

    WriteRawRC(ComIEnReg,irqEn|0x80);
    ClearBitMask(ComIrqReg,0x80);
    WriteRawRC(CommandReg,PCD_IDLE);
    SetBitMask(FIFOLevelReg,0x80);

    for(i=0;i<InLenByte;i++)
    {
        WriteRawRC(FIFODataReg,pInData[i]);
    }

    WriteRawRC(CommandReg,Command);

    if(Command==PCD_TRANSCEIVE)
        SetBitMask(BitFramingReg,0x80);

    uint32_t start=HAL_GetTick();

    do
    {
        n=ReadRawRC(ComIrqReg);
        //osDelay(1);
    }
    while(!(n&0x01) && !(n&waitFor) && ((HAL_GetTick()-start)<30));

    ClearBitMask(BitFramingReg,0x80);

    if(!(ReadRawRC(ErrorReg)&0x1B))
    {
        status=MI_OK;

        if(n&irqEn&0x01)
            status=MI_NOTAGERR;

        if(Command==PCD_TRANSCEIVE)
        {
            n=ReadRawRC(FIFOLevelReg);
            lastBits=ReadRawRC(ControlReg)&0x07;

            if(lastBits)
                *pOutLenBit=(n-1)*8+lastBits;
            else
                *pOutLenBit=n*8;

            if(n==0) n=1;
            if(n>MAXRLEN) n=MAXRLEN;

            for(i=0;i<n;i++)
                pOutData[i]=ReadRawRC(FIFODataReg);
        }
    }

    SetBitMask(ControlReg,0x80);
    WriteRawRC(CommandReg,PCD_IDLE);

    return status;
}

int8_t PcdRequest(uint8_t req_code,uint8_t *pTagType)
{
    int8_t status;
    uint16_t len;
    uint8_t buf[MAXRLEN];

    ClearBitMask(Status2Reg,0x08);
    WriteRawRC(BitFramingReg,0x07);
    SetBitMask(TxControlReg,0x03);

    buf[0]=req_code;

    status=PcdComMF522(PCD_TRANSCEIVE,buf,1,buf,&len);

    if(status==MI_OK && len==0x10)
    {
        pTagType[0]=buf[0];
        pTagType[1]=buf[1];
    }
    else
        status=MI_ERR;

    return status;
}

int8_t PcdAnticoll(uint8_t *pSnr)
{
    int8_t status;
    uint8_t i,snr_check=0;
    uint16_t len;
    uint8_t buf[MAXRLEN];

    ClearBitMask(Status2Reg,0x08);
    WriteRawRC(BitFramingReg,0x00);
    ClearBitMask(CollReg,0x80);

    buf[0]=PICC_ANTICOLL1;
    buf[1]=0x20;

    status=PcdComMF522(PCD_TRANSCEIVE,buf,2,buf,&len);

    if(status==MI_OK)
    {
        for(i=0;i<4;i++)
        {
            pSnr[i]=buf[i];
            snr_check^=buf[i];
        }

        if(snr_check!=buf[4])
            status=MI_ERR;
    }

    SetBitMask(CollReg,0x80);

    return status;
}

int8_t PcdAuthState(uint8_t auth_mode,uint8_t addr,uint8_t *pKey,uint8_t *pSnr)
{
    int8_t status;
    uint16_t len;
    uint8_t i,buf[MAXRLEN];

    buf[0]=auth_mode;
    buf[1]=addr;

    for(i=0;i<6;i++)
        buf[i+2]=pKey[i];

    for(i=0;i<4;i++)   // 修复：UID只有4字节
        buf[i+8]=pSnr[i];

    status=PcdComMF522(PCD_AUTHENT,buf,12,buf,&len);

    if(status!=MI_OK || !(ReadRawRC(Status2Reg)&0x08))
        status=MI_ERR;

    return status;
}

int8_t PcdSelect(uint8_t *pSnr)
{
    int8_t status;
    uint8_t i;
    uint16_t unLen;
    uint8_t buf[MAXRLEN]; 
    
    buf[0] = PICC_ANTICOLL1;
    buf[1] = 0x70;
    buf[6] = 0;
    for (i=0; i<4; i++)
    {
    	buf[i+2] = pSnr[i];
    	buf[6]  ^= pSnr[i];
    }
    CalulateCRC(buf, 7, &buf[7]);
  
    ClearBitMask(Status2Reg, 0x08);

    status = PcdComMF522(PCD_TRANSCEIVE, buf, 9, buf, &unLen);
    
    if ((status == MI_OK) && (unLen == 0x18))
    {   status = MI_OK;  }
    else
    {   status = MI_ERR;    }

    return status;
}

int8_t PcdRead(uint8_t addr, uint8_t *pData)
{
    int8_t status;
    uint16_t unLen;
    uint8_t i, buf[MAXRLEN]; 

    buf[0] = PICC_READ;
    buf[1] = addr;
    CalulateCRC(buf, 2, &buf[2]);
   
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 4, buf, &unLen);
    if ((status == MI_OK) && (unLen == 0x90))
    {
        for (i=0; i<16; i++)
        {    pData[i] = buf[i];   }
    }
    else
    {   status = MI_ERR;   }
    
    return status;
}

int8_t PcdWrite(uint8_t addr, uint8_t *pData)
{
    int8_t status;
    uint16_t unLen;
    uint8_t i, buf[MAXRLEN]; 
    
    buf[0] = PICC_WRITE;
    buf[1] = addr;
    CalulateCRC(buf, 2, &buf[2]);
 
    status = PcdComMF522(PCD_TRANSCEIVE, buf, 4, buf, &unLen);

    if ((status != MI_OK) || (unLen != 4) || ((buf[0] & 0x0F) != 0x0A))
    {   status = MI_ERR;   }
        
    if (status == MI_OK)
    {
        for (i=0; i<16; i++)
        {    buf[i] = pData[i];   }
        CalulateCRC(buf, 16, &buf[16]);

        status = PcdComMF522(PCD_TRANSCEIVE, buf, 18, buf, &unLen);
        if ((status != MI_OK) || (unLen != 4) || ((buf[0] & 0x0F) != 0x0A))
        {   status = MI_ERR;   }
    }
    
    return status;
}

int8_t PcdHalt(void)
{
    int8_t status;
    uint16_t len;
    uint8_t buf[MAXRLEN];

    buf[0]=PICC_HALT;
    buf[1]=0;

    CalulateCRC(buf,2,&buf[2]);

    status=PcdComMF522(PCD_TRANSCEIVE,buf,4,buf,&len);

    return status;
}




/**
 * @brief  向 M1 卡的指定块写入 16 字节数据
 * @param  uid: 4字节卡号 (由 PcdAnticoll 获取)
 * @param  blockAddr: 要写入的绝对块号 (0~63)
 * @param  key: 6字节扇区密钥 (通常是 A 密钥)
 * @param  writeData: 准备写入的 16 字节数据数组
 * @return MI_OK (0) 表示成功，其他值表示失败
 */
int8_t M1_WriteCardBlock(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *writeData)
{
    int8_t status;

    // 1. 选卡：与目标卡片建立专属通信
    status = PcdSelect(uid);
    if (status != MI_OK) return status;

    // 2. 验证密钥：使用 A 密钥 (PICC_AUTHENT1A) 验证指定块
    status = PcdAuthState(PICC_AUTHENT1A, blockAddr, key, uid);
    if (status != MI_OK) return status;

    // 3. 写入数据：向指定块写入 16 字节
    status = PcdWrite(blockAddr, writeData);
    if (status != MI_OK) return status;

    // 4. 休眠卡片：释放卡片，等待下次寻卡
    PcdHalt();

    return MI_OK;
}

/**
 * @brief  从 M1 卡的指定块读取 16 字节数据
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

    // 2. 验证密钥
    status = PcdAuthState(PICC_AUTHENT1A, blockAddr, key, uid);
    if (status != MI_OK) return status;

    // 3. 读取数据
    status = PcdRead(blockAddr, readData);
    if (status != MI_OK) return status;

    // 4. 休眠卡片
    PcdHalt();

    return MI_OK;
}