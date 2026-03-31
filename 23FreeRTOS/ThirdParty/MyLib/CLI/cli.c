#include "cli.h"
#include <string.h>

/**
 * @brief 初始化 CLI 实例
 * @param cli   CLI 实例指针
 * @param table 用户定义的命令路由表（数组）
 * @param count 路由表中的命令数量
 */
void CLI_Init(cli_t *cli, const cli_cmd_entry_t *table, uint16_t count)
{
    if (!cli) return;
    
    // 绑定命令路由表
    cli->cmd_table = table;
    cli->cmd_count = count;
    
    // 强制复位内部所有状态，确保不会因为内存遗留垃圾数据导致误判
    CLI_Reset(cli);
}

/**
 * @brief 复位 CLI 解析器状态
 * @note 当通信信道断开重连，或者需要强行终止当前解析时调用
 */
void CLI_Reset(cli_t *cli)
{
    if (!cli) return;
    
    cli->state = CLI_STATE_IDLE; // 回到静默等待状态
    
    // 清零各种匹配游标和长度记录
    cli->preempt_idx = 0;
    cli->arg_match_idx = 0;
    cli->cmd_len = 0;
    cli->arg_len = 0;
}

/**
 * @brief 在绑定的路由表中查找对应的命令
 * @param cmd 提取出的命令名称字符串（不含换行符）
 * @param len 命令名称的长度
 * @return 匹配的表项指针，未找到则返回 NULL
 */
const cli_cmd_entry_t* CLI_FindCommand(cli_t *cli, const char *cmd, size_t len)
{
    if (!cli || !cmd) return NULL;
    
    for (uint16_t i = 0; i < cli->cmd_count; i++)
    {
        const char *table_cmd = cli->cmd_table[i].cmd;
        
        // 核心逻辑：先比对长度，再比对内容。
        // 为什么要比对长度？防止 "LED" 错误地匹配到表中的 "LED_ON"
        if (strlen(table_cmd) == len && strncmp(table_cmd, cmd, len) == 0)
        {
            return &cli->cmd_table[i];
        }
    }
    return NULL;
}

/**
 * @brief CLI 流式数据输入与解析核心引擎
 * @param data 接收到的数据片段（可以是1个字节，也可以是100个字节）
 * @param len  数据片段的长度
 */
void CLI_Input(cli_t *cli, const char *data, size_t len)
{
    if (!cli || !data) return;

    // 逐字节吞吐数据，绝不假设数据是完整的一包
    for (size_t i = 0; i < len; i++)
    {
        char c = data[i];

        /* ====================================================================
         * 【机制一：上帝视角抢占器 (Global Preemptor)】
         * 目标：在任何时刻、任何状态下，只要接收流中拼凑出了 "@CMD " 这5个字符，
         * 就立刻推翻当前一切工作，重新开始解析新命令。
         * 优势：极其强大的容错恢复能力，免疫任何网络粘包、残缺包。
         * ==================================================================== */
        const char *preempt_str = "@CMD ";
        
        if (c == preempt_str[cli->preempt_idx]) 
        {
            // 当前字符匹配上了 "@CMD " 中的某一个字符，游标推进
            cli->preempt_idx++;
            
            if (cli->preempt_idx == 5) 
            {
                // 完美凑齐了 5 个字符 "@CMD "！触发系统级抢占复位！
                cli->state = CLI_STATE_CMD_NAME; // 强制进入命令名接收状态
                cli->cmd_len = 0;                // 清空命令名缓存
                cli->preempt_idx = 0;            // 重置抢占游标，准备下一次拦截
                continue;                        // 关键：跳过下方状态机，直接处理下一个字符
            }
        } 
        else 
        {
            // 匹配中途中断了（比如收到了 "@CMX"）
            // 容错处理：如果当前导致中断的字符恰好又是一个 '@'，则将游标设为1，
            // 否则游标清零，重新等待新的 '@'。
            cli->preempt_idx = (c == '@') ? 1 : 0;
        }

        /* ====================================================================
         * 【机制二：主状态机流转 (Main State Machine)】
         * 负责按照 @CMD -> name -> @ARG -> payload -> @END 的顺序严格组装数据。
         * ==================================================================== */
        switch (cli->state)
        {
        case CLI_STATE_IDLE:
            // 【空闲状态】：什么都不做。垃圾数据全被丢弃。
            // 只有上方的“上帝视角抢占器”匹配到 "@CMD " 时，才会把状态切走。
            break;

        case CLI_STATE_CMD_NAME:
            // 【接收命令名状态】：提取指令名称，直到遇到换行符
            if (c == '\r') continue; // 兼容 Windows 系统的 \r\n 换行符，直接丢弃 \r
            
            if (c == '\n') 
            {
                // 遇到换行，说明命令名字段结束
                cli->cmd_buf[cli->cmd_len] = '\0'; // 字符串封口
                cli->state = CLI_STATE_WAIT_ARG;   // 状态切换：准备寻找参数块头部
                cli->arg_match_idx = 0;            // 初始化参数块头部的匹配游标
            } 
            else if (cli->cmd_len < CLI_MAX_CMD_LEN - 1) 
            {
                // 只要还没满，就把字符塞进命令名缓存里
                cli->cmd_buf[cli->cmd_len++] = c;
            }
            break;

        case CLI_STATE_WAIT_ARG:
            // 【等待参数头部状态】：使用独立的游标匹配 "@ARG"
            {
                const char *arg_str = "@ARG";
                if (c == arg_str[cli->arg_match_idx]) 
                {
                    cli->arg_match_idx++;
                    if (cli->arg_match_idx == 4) 
                    {
                        // 成功匹配 "@ARG"，但还要确保它后面跟着换行符
                        cli->state = CLI_STATE_WAIT_ARG_NL;
                    }
                } 
                else 
                {
                    // 匹配失败，滑动窗口回退逻辑（同抢占器）
                    cli->arg_match_idx = (c == '@') ? 1 : 0;
                }
            }
            break;

        case CLI_STATE_WAIT_ARG_NL:
            // 【严格校验参数头状态】："@ARG" 后面必须是换行，不允许出现 "@ARGHH"
            if (c == '\r') continue;
            
            if (c == '\n') 
            {
                // 校验通过！正式开启“盲收多行参数”模式
                cli->state = CLI_STATE_ARG_PAYLOAD;
                cli->arg_len = 0; // 重置参数缓存长度
            } 
            else 
            {
                // 格式不规范，抛弃当前命令，退回空闲状态等待下一个 @CMD
                cli->state = CLI_STATE_IDLE; 
            }
            break;

        case CLI_STATE_ARG_PAYLOAD:
            // 【盲收参数负载状态】：无脑接收所有字符，同时用“滑动窗口”探测尾部的 "@END"
            if (cli->arg_len < CLI_MAX_ARG_LEN - 1) 
            {
                // 1. 无脑存入当前字符（哪怕它是乱码、括号、换行符）
                cli->arg_buf[cli->arg_len++] = c;

                // 2. 尾部滑动窗口探测（Look-behind matching）
                // 只要当前缓冲区长度大于等于4，我们就检查刚刚收到的最后4个字符
                // 是不是正好拼成了 "@END"
                if (cli->arg_len >= 4 &&
                    cli->arg_buf[cli->arg_len - 4] == '@' &&
                    cli->arg_buf[cli->arg_len - 3] == 'E' &&
                    cli->arg_buf[cli->arg_len - 2] == 'N' &&
                    cli->arg_buf[cli->arg_len - 1] == 'D') 
                {
                    // 找到了 "@END"！说明参数块结束了。
                    
                    // 核心动作：把 "@END" 这 4 个字符从参数缓存中抹掉，替换为 '\0'
                    // 这样交给用户的 arg_buf 里就是纯净的参数内容
                    cli->arg_buf[cli->arg_len - 4] = '\0';

                    // 此时，cmd_buf 和 arg_buf 都已准备就绪。开始执行回调！
                    const cli_cmd_entry_t *entry = CLI_FindCommand(cli, cli->cmd_buf, cli->cmd_len);
                    if (entry && entry->handler) 
                    {
                        // 将干净的字符串传递给具体的业务处理函数
                        entry->handler(cli->cmd_buf, cli->arg_buf);
                    }

                    // 完美谢幕，状态机复位，安静等待下一条命令
                    cli->state = CLI_STATE_IDLE;
                }
            }
            else
            {
                 // 缓冲区满了都没找到 "@END"，为了防止死锁，强制复位
                 // 业务上可以在这里加个打印报警："Error: Argument buffer overflow"
                 cli->state = CLI_STATE_IDLE;
            }
            break;
            
        default:
            // 异常状态捕获，强制归位
            cli->state = CLI_STATE_IDLE;
            break;
        }
    }
}