#include "sdram.h"

/**
  * @brief  向 SDRAM 发送命令
  * @param  cmd: 命令模式
  * @param  bank: 目标 Bank
  * @param  autoRefresh: 自动刷新次数
  * @param  modeReg: 模式寄存器配置
  */
static void SDRAM_SendCommand(uint32_t cmd, uint32_t bank, uint32_t autoRefresh, uint32_t modeReg)
{
    FMC_SDRAM_CommandTypeDef Command;

    Command.CommandMode            = cmd;
    Command.CommandTarget          = bank;
    Command.AutoRefreshNumber      = autoRefresh;
    Command.ModeRegisterDefinition = modeReg;

    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);
}

/**
  * @brief  执行 SDRAM 初始化序列 (必须在 MX_FMC_Init 后调用)
  */
void SDRAM_InitSequence(void)
{
    uint32_t mode;

    /* Step 1: 开启 SDRAM 时钟 (CLK Enable) */
    SDRAM_SendCommand(FMC_SDRAM_CMD_CLK_ENABLE, FMC_SDRAM_CMD_TARGET_BANK1, 1, 0);

    /* 等待时钟稳定，手册要求至少 100us，这里给 1ms */
    HAL_Delay(1);

    /* Step 2: 对所有 Bank 预充电 (Precharge All) */
    SDRAM_SendCommand(FMC_SDRAM_CMD_PALL, FMC_SDRAM_CMD_TARGET_BANK1, 1, 0);

    /* Step 3: 自动刷新 (Auto Refresh) - 至少需要执行两次，这里执行 8 次确保稳定 */
    SDRAM_SendCommand(FMC_SDRAM_CMD_AUTOREFRESH_MODE, FMC_SDRAM_CMD_TARGET_BANK1, 8, 0);

    /* Step 4: 配置模式寄存器 (Load Mode Register) 
     * 对应 CubeMX 中的配置: CAS Latency = 2, Burst Length = 1
     */
    mode = SDRAM_MODEREG_BURST_LENGTH_1 | 
           SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL | 
           SDRAM_MODEREG_CAS_LATENCY_2 | 
           SDRAM_MODEREG_OPERATING_MODE_STANDARD | 
           SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    SDRAM_SendCommand(FMC_SDRAM_CMD_LOAD_MODE, FMC_SDRAM_CMD_TARGET_BANK1, 1, mode);

    /* Step 5: 设置刷新率计数器 
     * W9825G6KH 刷新周期: 64ms 内需进行 8192 次刷新
     * 计算公式: COUNT = (SDRAM时钟频率 * 64ms / 8192) - 20
     * 假设 MCU 主频 180MHz，SDRAM 配置为 2 分频，则 SDRAM 时钟为 90MHz:
     * COUNT = (90,000,000 * 0.0000078125) - 20 = 703 - 20 = 683
     */
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, 683); 
    
    // 注意：如果你的系统配置是 120MHz 的 SDRAM 时钟，则 COUNT 约为 917。请根据实际时钟树修改。
}

/* ================== 下方为读写接口封装 ================== */

void SDRAM_WriteBuffer8(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t *p = (uint8_t *)(SDRAM_BANK_ADDR + addr);
    for(uint32_t i = 0; i < len; i++) {
        p[i] = data[i];
    }
}

void SDRAM_ReadBuffer8(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t *p = (uint8_t *)(SDRAM_BANK_ADDR + addr);
    for(uint32_t i = 0; i < len; i++) {
        data[i] = p[i];
    }
}

void SDRAM_WriteBuffer16(uint32_t addr, uint16_t *data, uint32_t len)
{
    uint16_t *p = (uint16_t *)(SDRAM_BANK_ADDR + addr);
    for(uint32_t i = 0; i < len; i++) {
        p[i] = data[i];
    }
}

void SDRAM_ReadBuffer16(uint32_t addr, uint16_t *data, uint32_t len)
{
    uint16_t *p = (uint16_t *)(SDRAM_BANK_ADDR + addr);
    for(uint32_t i = 0; i < len; i++) {
        data[i] = p[i];
    }
}

void SDRAM_WriteBuffer32(uint32_t addr, uint32_t *data, uint32_t len)
{
    uint32_t *p = (uint32_t *)(SDRAM_BANK_ADDR + addr);
    for(uint32_t i = 0; i < len; i++) {
        p[i] = data[i];
    }
}

void SDRAM_ReadBuffer32(uint32_t addr, uint32_t *data, uint32_t len)
{
    uint32_t *p = (uint32_t *)(SDRAM_BANK_ADDR + addr);
    for(uint32_t i = 0; i < len; i++) {
        data[i] = p[i];
    }
}

/**
  * @brief  SDRAM 读写完整性测试
  * @retval 0: 测试通过; 1: 32位读写失败; 2: 16位读写失败; 3: 8位读写失败
  */
uint8_t SDRAM_Test(void)
{
    uint32_t i;
    // 为了防止看门狗复位或等待过久，这里测试前 256KB 的空间作为快速验证
    // 你可以将其修改为 SDRAM_SIZE (0x2000000) 进行全盘深度测试
    uint32_t test_length = 0x40000; 
    
    uint32_t *ptr32;
    uint16_t *ptr16;
    uint8_t  *ptr8;

    /* -------------------------------------------------------------------
     * 1. 32-bit (Word) 读写测试
     * ------------------------------------------------------------------- */
    ptr32 = (uint32_t *)SDRAM_BANK_ADDR;
    // 写入特种测试数据 (交替的 1 和 0) 以及偏移量，最容易暴露出数据线短路问题
    for (i = 0; i < test_length / 4; i++) {
        ptr32[i] = 0x55AA55AA + i; 
    }
    // 读取校验
    for (i = 0; i < test_length / 4; i++) {
        if (ptr32[i] != (0x55AA55AA + i)) {
            return 1; // 32位测试失败
        }
    }

    /* -------------------------------------------------------------------
     * 2. 16-bit (Half-Word) 读写测试
     * ------------------------------------------------------------------- */
    ptr16 = (uint16_t *)SDRAM_BANK_ADDR;
    for (i = 0; i < test_length / 2; i++) {
        ptr16[i] = (uint16_t)(0xAA55 + i);
    }
    for (i = 0; i < test_length / 2; i++) {
        if (ptr16[i] != (uint16_t)(0xAA55 + i)) {
            return 2; // 16位测试失败
        }
    }

    /* -------------------------------------------------------------------
     * 3. 8-bit (Byte) 读写测试
     * ------------------------------------------------------------------- */
    ptr8 = (uint8_t *)SDRAM_BANK_ADDR;
    for (i = 0; i < test_length; i++) {
        ptr8[i] = (uint8_t)(0xA5 + i);
    }
    for (i = 0; i < test_length; i++) {
        if (ptr8[i] != (uint8_t)(0xA5 + i)) {
            return 3; // 8位测试失败
        }
    }

    return 0; // 测试全通过
}