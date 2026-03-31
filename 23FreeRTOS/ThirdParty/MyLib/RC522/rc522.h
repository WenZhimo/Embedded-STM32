#ifndef __RC522_H
#define __RC522_H

#include "main.h"          // 引入 CubeMX 生成的引脚宏定义 (RC522_CS_Pin 等)
#include "stm32f4xx_hal.h" // 引入 HAL 库

// 和MF522通讯时返回的错误代码
#define MI_OK                          0
#define MI_NOTAGERR                    (-1)
#define MI_ERR                         (-2)
#define MI_ACCESSERR                   (-3)   // 权限不足或拒绝访问控制块
#define MI_FORMATERR                   (-4)   // 格式错误：控制块的控制位校验不通过 (原码与反码不匹配)




//=============== 配置区 ===============

#define M1_AUTO_HALT_ENABLE
//封装行为宏定义
//封装读写函数（M1开头的）会再读写完毕后自动执行 PcdHalt() 让卡片休眠，防止一直重复读写
//用户可以根据宏定义来使能或禁止自动休眠


#define RF_USE_RTOS  // 定义这个宏以启用 FreeRTOS 版本的函数 (osDelay)，否则使用 HAL_Delay





//============ 命令字和寄存器定义 ============
// MF522命令字
#define PCD_IDLE              0x00               //取消当前命令
#define PCD_AUTHENT           0x0E               //验证密钥
#define PCD_RECEIVE           0x08               //接收数据
#define PCD_TRANSMIT          0x04               //发送数据
#define PCD_TRANSCEIVE        0x0C               //发送并接收数据
#define PCD_RESETPHASE        0x0F               //复位
#define PCD_CALCCRC           0x03               //CRC计算

// Mifare_One卡片命令字
#define PICC_REQIDL           0x26               //寻天线区内未进入休眠状态
#define PICC_REQALL           0x52               //寻天线区内全部卡
#define PICC_ANTICOLL1        0x93               //防冲撞
#define PICC_ANTICOLL2        0x95               //防冲撞
#define PICC_AUTHENT1A        0x60               //验证A密钥
#define PICC_AUTHENT1B        0x61               //验证B密钥
#define PICC_READ             0x30               //读块
#define PICC_WRITE            0xA0               //写块
#define PICC_DECREMENT        0xC0               //扣款
#define PICC_INCREMENT        0xC1               //充值
#define PICC_RESTORE          0xC2               //调块数据到缓冲区
#define PICC_TRANSFER         0xB0               //保存缓冲区中数据
#define PICC_HALT             0x50               //休眠

// MF522寄存器定义
// PAGE 0
#define     RFU00                 0x00    
#define     CommandReg            0x01    
#define     ComIEnReg             0x02    
#define     DivlEnReg             0x03    
#define     ComIrqReg             0x04    
#define     DivIrqReg             0x05
#define     ErrorReg              0x06    
#define     Status1Reg            0x07    
#define     Status2Reg            0x08    
#define     FIFODataReg           0x09
#define     FIFOLevelReg          0x0A
#define     WaterLevelReg         0x0B
#define     ControlReg            0x0C
#define     BitFramingReg         0x0D
#define     CollReg               0x0E
#define     RFU0F                 0x0F
// PAGE 1     
#define     RFU10                 0x10
#define     ModeReg               0x11
#define     TxModeReg             0x12
#define     RxModeReg             0x13
#define     TxControlReg          0x14
#define     TxAutoReg             0x15
#define     TxSelReg              0x16
#define     RxSelReg              0x17
#define     RxThresholdReg        0x18
#define     DemodReg              0x19
#define     RFU1A                 0x1A
#define     RFU1B                 0x1B
#define     MifareReg             0x1C
#define     RFU1D                 0x1D
#define     RFU1E                 0x1E
#define     SerialSpeedReg        0x1F
// PAGE 2    
#define     RFU20                 0x20  
#define     CRCResultRegM         0x21
#define     CRCResultRegL         0x22
#define     RFU23                 0x23
#define     ModWidthReg           0x24
#define     RFU25                 0x25
#define     RFCfgReg              0x26
#define     GsNReg                0x27
#define     CWGsCfgReg            0x28
#define     ModGsCfgReg           0x29
#define     TModeReg              0x2A
#define     TPrescalerReg         0x2B
#define     TReloadRegH           0x2C
#define     TReloadRegL           0x2D
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F
// PAGE 3      
#define     RFU30                 0x30
#define     TestSel1Reg           0x31
#define     TestSel2Reg           0x32
#define     TestPinEnReg          0x33
#define     TestPinValueReg       0x34
#define     TestBusReg            0x35
#define     AutoTestReg           0x36
#define     VersionReg            0x37
#define     AnalogTestReg         0x38
#define     TestDAC1Reg           0x39  
#define     TestDAC2Reg           0x3A   
#define     TestADCReg            0x3B   
#define     RFU3C                 0x3C   
#define     RFU3D                 0x3D   
#define     RFU3E                 0x3E   
#define     RFU3F		          0x3F

//========================================
/* ================= M1卡控制位魔法字典 (Access Bits) ================= */

// 1. 自由开发模式：KeyA/B均可任意读写 (出厂默认)
extern uint8_t CTRL_FREE_DEV[4]  ;     

// 2. 读写分离模式：KeyA只读，KeyB可读写且可修改密码
extern uint8_t CTRL_MASTER_SLAVE[4] ;   

// 3. 防篡改只读模式：KeyA/B均只读，仅KeyB可修改控制位，写时需要先用B修改控制位再写入数据
extern uint8_t CTRL_ANTI_TAMPER[4] ;    

// 4. 安全储值卡模式：KeyA仅支持查询/扣款，KeyB支持充值/初始化
extern uint8_t CTRL_SECURE_WALLET[4] ;  





//出厂默认密钥
extern uint8_t M1_DefaultKey[6];
// MAD扇区默认密钥 (NDEF卡扇区0的控制块)
extern uint8_t Key_MAD[6];
// NDEF扇区默认密钥 (NDEF卡扇区1~15的控制块)
extern uint8_t Key_NDEF[6];

// 函数原型声明

// RC522 硬件与软件复位及初始化
int8_t PcdReset(void);                                                                                                                        
// 开启天线发射
void PcdAntennaOn(void);                                                             
// 关闭天线发射
void PcdAntennaOff(void);                                                            
// 寻卡：寻找天线工作区内的卡片
int8_t PcdRequest(unsigned char req_code,unsigned char *pTagType);                   
// 防冲突：检测所有卡并获取其中一张的 UID
int8_t PcdAnticoll(unsigned char *pSnr);                                             
// 选卡：将焦点锁定在指定 UID 的卡片上
int8_t PcdSelect(unsigned char *pSnr);                                               
// 验证扇区密码，建立加密通信管道
int8_t PcdAuthState(unsigned char auth_mode,unsigned char addr,unsigned char *pKey,unsigned char *pSnr); 
// 读取卡片块数据 (读取已认证块的16字节)
int8_t PcdRead(unsigned char addr,unsigned char *pData);                             
// 写入卡片块数据 (向已认证块写入16字节)
int8_t PcdWrite(unsigned char addr,unsigned char *pData);                            
// 钱包充值/扣款等操作
int8_t PcdValue(unsigned char dd_mode,unsigned char addr,unsigned char *pValue);     
// 块数据备份/调配
int8_t PcdBakValue(unsigned char sourceaddr, unsigned char goaladdr);                
//主动关闭 RC522 的 Crypto1 硬件加密引擎
void PcdStopCrypto1(void);
// 使目标卡片进入休眠状态
int8_t PcdHalt(void);                                                                
// RC522 与卡片通信的核心底层函数(状态机)
int8_t PcdComMF522(uint8_t Command,uint8_t *pInData,uint8_t InLenByte,uint8_t *pOutData,uint16_t *pOutLenBit); 
// 利用 RC522 的硬件协处理器计算 CRC 校验码
void CalulateCRC(uint8_t *pIndata,uint8_t len,uint8_t *pOutData);                    
// 通过 SPI 向 RC522 的寄存器写入数据
void WriteRawRC(uint8_t Address,uint8_t value);                                      
// 通过 SPI 读取 RC522 的寄存器
uint8_t ReadRawRC(uint8_t Address);                                                  
// 置位 RC522 寄存器的特定位 (按位或)
void SetBitMask(uint8_t reg,uint8_t mask);                                           
// 清零 RC522 寄存器的特定位 (按位与非)
void ClearBitMask(uint8_t reg,uint8_t mask);                                         

// 【封装】强制唤醒 M1 卡并获取 UID
int8_t M1_ForceWakeUp(uint8_t *uid);
// 【封装】从 M1 卡的指定块写 16 字节数据
int8_t M1_WriteCardBlock(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *writeData);               
// 【封装】从 M1 卡的指定块读取 16 字节数据
int8_t M1_ReadCardBlock(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *readData);                 
// 从 M1 卡读取 16 字节，支持选择验证 Key A 还是 Key B
int8_t M1_ReadCardBlock_Ext(uint8_t *uid, uint8_t blockAddr, uint8_t auth_mode, uint8_t *key, uint8_t *readData); 
// 向 M1 卡写入 16 字节，支持选择验证 Key A 还是 Key B
int8_t M1_WriteCardBlock_Ext(uint8_t *uid, uint8_t blockAddr, uint8_t auth_mode, uint8_t *key, uint8_t *writeData); 

// [绝对安全写] 向指定绝对块地址写入数据，严格拒绝写控制块
int8_t M1_WriteDataBlock_Abs(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *writeData);           
// [绝对读] 从指定绝对块地址读取数据
int8_t M1_ReadDataBlock_Abs(uint8_t *uid, uint8_t blockAddr, uint8_t *key, uint8_t *readData);             
// [相对安全写] 指定扇区号和相对数据块号写入数据,，严格拒绝写控制块
int8_t M1_WriteDataBlock_Rel(uint8_t *uid, uint8_t sector, uint8_t relBlock, uint8_t *key, uint8_t *writeData); 
// [相对读] 指定扇区号和相对数据块号读取数据
int8_t M1_ReadDataBlock_Rel(uint8_t *uid, uint8_t sector, uint8_t relBlock, uint8_t *key, uint8_t *readData);   
// [高危写] 修改指定扇区的密码和访问控制位，带防砖校验
int8_t M1_WriteSectorControlBlock_Safe(uint8_t *uid, uint8_t sector, uint8_t *key, uint8_t *newControlData);    

// 构建 M1 卡控制块数据 (16字节)
void Make_SectorTrailer(uint8_t *newKeyA, uint8_t *newKeyB, uint8_t *ctrlBits, uint8_t *outTrailer);       












/*

uint8_t CardType[2];
  uint8_t CardUID[4];
  uint8_t MyReadBuffer[16];
  uint8_t MyWriteData[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  int8_t status;
// --- 写入测试 (写入所有块) ---
           
            for (int i = 0; i < 64; i++) 
            {
                // 1. 唤醒步骤：每次物理操作前，必须重新寻卡和防冲突！
                if (PcdRequest(PICC_REQALL, CardType) != MI_OK) {
                    printf("块 %02d : 寻卡失败，卡片可能不在感应区或已移开\r\n", i);
                    osDelay(50);
                    continue; // 唤醒失败，跳过本次循环
                }

                if (PcdAnticoll(CardUID) != MI_OK) {
                    printf("块 %02d : 防冲突失败\r\n", i);
                    osDelay(50);
                    continue; 
                }

                // 2. 执行安全写入
                status = M1_WriteDataBlock_Abs(CardUID, i, M1_DefaultKey, MyWriteData);

                // 3. 根据状态码精准打印日志
                if (status == MI_OK) {
                    printf("块 %02d : 成功！数据已写入。\r\n", i);
                } 
                else if (status == MI_ACCESSERR) {
                    // 遇到块 0 或 3, 7, 11 等控制块，安全机制生效被跳过
                    printf("块 %02d : 跳过！(厂商只读块 或 密码控制块)\r\n", i);
                } 
                else {
                    // 底层通信失败或密码验证失败
                    printf("块 %02d : 失败！密码错误或扇区损坏。\r\n", i);
                }

                // 给卡片状态机留一点点恢复的缓冲时间
                HAL_Delay(10); 
                PcdHalt(); 
            }


            // --- 读取测试 (读取所有 64 块) ---
            for (int i = 0; i < 64; i++) 
            {
                // 1. 强力唤醒卡片
                if (PcdRequest(PICC_REQALL, CardType) != MI_OK) 
                {
                    printf("块 %02d : 寻卡失败\r\n", i);
                    osDelay(10);
                    continue;
                }

                // 2. 防冲突获取 UID
                if (PcdAnticoll(CardUID) != MI_OK) 
                {
                    continue; 
                }

                // 3. 一招鲜吃遍天：直接用默认的出厂密码 M1_DefaultKey 读取
                status = M1_ReadDataBlock_Abs(CardUID, i, M1_DefaultKey, MyReadBuffer);

                // 4. 格式化输出结果
                if (status == MI_OK) 
                {
                    printf("块 %02d [数据] : ", i);
                    for (int j = 0; j < 16; j++) {
                        printf("%02X ", MyReadBuffer[j]);
                    }
                    printf("\r\n");
                } 
                else if (status == MI_ACCESSERR) 
                {
                    // 自动拦截第 3、7、11 等密码控制块，防止读出全 00 的无用数据
                    printf("块 %02d [控制] : --- 密码控制块 (受保护) ---\r\n", i);
                } 
                else 
                {
                    printf("块 %02d [错误] : 验证或读取失败\r\n", i);
                }

                HAL_Delay(10); 
            }
            



*/
#endif