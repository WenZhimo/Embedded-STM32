#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* ---------------- 配置参数 ---------------- */

#ifndef CLI_BUFFER_SIZE
#define CLI_BUFFER_SIZE 256
#endif

#ifndef CLI_MAX_CMD_LEN
#define CLI_MAX_CMD_LEN 32
#endif

#ifndef CLI_MAX_ARG_LEN
#define CLI_MAX_ARG_LEN 128
#endif


/* ---------------- 类型定义 ---------------- */

typedef struct cli cli_t;

/* 命令处理函数
 * cmd  : 命令字符串
 * args : 参数字符串
 */
typedef void (*cli_cmd_handler_t)(const char *cmd, const char *args);


/* 命令表项 */
typedef struct
{
    const char *cmd;
    cli_cmd_handler_t handler;
} cli_cmd_entry_t;


/* CLI 实例结构 */
struct cli
{
    char buffer[CLI_BUFFER_SIZE];
    uint16_t index;

    const cli_cmd_entry_t *cmd_table;
    uint16_t cmd_count;
};


/* ---------------- 接口函数 ---------------- */

/* 初始化 CLI 实例 */
void CLI_Init(cli_t *cli,const cli_cmd_entry_t *table,uint16_t count);


/* 清空解析缓冲区 */
void CLI_Reset(cli_t *cli);


/* 输入字符串进行解析
 * data : 输入字符串
 * len  : 字符串长度
 *
 * 可包含多个命令，例如：
 * >>cmd1<<{arg1}>>cmd2<<{arg2}
 */
void CLI_Input(cli_t *cli,const char *data,size_t len);


/* 直接解析完整字符串（不使用内部累积） */
void CLI_ParseLine(cli_t *cli,const char *line);


/* 在命令表中查找命令 */
const cli_cmd_entry_t* CLI_FindCommand(cli_t *cli,const char *cmd,size_t len);


/* 解析单条命令
 * 格式:
 * >>cmd<<{args}
 */
int CLI_ParseCommand(cli_t *cli,const char *cmd_begin,const char *cmd_end,const char *arg_begin,const char *arg_end);


#ifdef __cplusplus
}
#endif

#endif