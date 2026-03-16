#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- 配置参数 ---------------- */

#ifndef CLI_MAX_CMD_LEN
#define CLI_MAX_CMD_LEN 32      // 命令名称最大长度
#endif

#ifndef CLI_MAX_ARG_LEN
#define CLI_MAX_ARG_LEN 512     // 参数缓冲区最大长度（因为支持多行和JSON，建议调大）
#endif

/* ---------------- 类型定义 ---------------- */

typedef struct cli cli_t;

/* * 命令处理回调函数原型 */
typedef void (*cli_cmd_handler_t)(const char *cmd, const char *args);

/* * 命令表项结构 */
typedef struct
{
    const char *cmd;
    cli_cmd_handler_t handler;
} cli_cmd_entry_t;

/* * 解析器的内部状态机枚举 */
typedef enum
{
    CLI_STATE_IDLE = 0,         // 闲置状态，等待全局的 "@CMD " 唤醒
    CLI_STATE_CMD_NAME,         // 正在记录命令名称，直到遇到 '\n'
    CLI_STATE_WAIT_ARG,         // 正在寻找 "@ARG" 关键字
    CLI_STATE_WAIT_ARG_NL,      // 找到 "@ARG" 后，等待换行符 '\n'
    CLI_STATE_ARG_PAYLOAD       // 正在盲收多行参数，同时监听尾部是否出现 "@END"
} cli_state_t;

/* * CLI 实例结构 */
struct cli
{
    const cli_cmd_entry_t *cmd_table;
    uint16_t cmd_count;

    cli_state_t state;                 
    
    // --- 新增：用于关键字匹配的游标 ---
    uint8_t preempt_idx;               // 专门用于在任何时候截获 "@CMD " 的游标
    uint8_t arg_match_idx;             // 用于匹配 "@ARG" 的游标
    
    char cmd_buf[CLI_MAX_CMD_LEN];     
    char arg_buf[CLI_MAX_ARG_LEN];     
    uint16_t cmd_len;                  
    uint16_t arg_len;                  
};

/* ---------------- 接口函数 ---------------- */

void CLI_Init(cli_t *cli, const cli_cmd_entry_t *table, uint16_t count);
void CLI_Reset(cli_t *cli);
void CLI_Input(cli_t *cli, const char *data, size_t len);
const cli_cmd_entry_t* CLI_FindCommand(cli_t *cli, const char *cmd, size_t len);

#ifdef __cplusplus
}
#endif

#endif // CLI_H