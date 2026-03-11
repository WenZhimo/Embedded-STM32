#include "cli.h"
#include <string.h>


/* 内部状态 */
typedef enum
{
    CLI_STATE_IDLE = 0,
    CLI_STATE_CMD_START1,
    CLI_STATE_CMD,
    CLI_STATE_CMD_END1,
    CLI_STATE_ARG_START,
    CLI_STATE_ARG,
} cli_state_t;



void CLI_Init(cli_t *cli,
              const cli_cmd_entry_t *table,
              uint16_t count)
{
    if (!cli)
        return;

    cli->index = 0;
    cli->cmd_table = table;
    cli->cmd_count = count;
}



void CLI_Reset(cli_t *cli)
{
    if (!cli)
        return;

    cli->index = 0;
}



const cli_cmd_entry_t* CLI_FindCommand(cli_t *cli,
                                       const char *cmd,
                                       size_t len)
{
    if (!cli || !cmd)
        return NULL;

    for (uint16_t i = 0; i < cli->cmd_count; i++)
    {
        const char *table_cmd = cli->cmd_table[i].cmd;

        if (strlen(table_cmd) == len &&
            strncmp(table_cmd, cmd, len) == 0)
        {
            return &cli->cmd_table[i];
        }
    }

    return NULL;
}



int CLI_ParseCommand(cli_t *cli,
                     const char *cmd_begin,
                     const char *cmd_end,
                     const char *arg_begin,
                     const char *arg_end)
{
    if (!cli || !cmd_begin || !cmd_end)
        return -1;

    char cmd[CLI_MAX_CMD_LEN];
    char args[CLI_MAX_ARG_LEN];

    size_t cmd_len = cmd_end - cmd_begin;
    size_t arg_len = 0;

    if (cmd_len >= CLI_MAX_CMD_LEN)
        cmd_len = CLI_MAX_CMD_LEN - 1;

    memcpy(cmd, cmd_begin, cmd_len);
    cmd[cmd_len] = '\0';

    if (arg_begin && arg_end && arg_end > arg_begin)
    {
        arg_len = arg_end - arg_begin;

        if (arg_len >= CLI_MAX_ARG_LEN)
            arg_len = CLI_MAX_ARG_LEN - 1;

        memcpy(args, arg_begin, arg_len);
        args[arg_len] = '\0';
    }
    else
    {
        args[0] = '\0';
    }

    const cli_cmd_entry_t *entry = CLI_FindCommand(cli, cmd, cmd_len);

    if (entry && entry->handler)
    {
        entry->handler(cmd, args);
        return 0;
    }

    return -1;
}



void CLI_ParseLine(cli_t *cli,
                   const char *line)
{
    if (!cli || !line)
        return;

    cli_state_t state = CLI_STATE_IDLE;

    const char *cmd_begin = NULL;
    const char *cmd_end = NULL;
    const char *arg_begin = NULL;
    const char *arg_end = NULL;

    const char *p = line;

    while (*p)
    {
        char c = *p;

        switch (state)
        {

        case CLI_STATE_IDLE:

            if (c == '>')
                state = CLI_STATE_CMD_START1;

            break;


        case CLI_STATE_CMD_START1:

            if (c == '>')
            {
                cmd_begin = p + 1;
                state = CLI_STATE_CMD;
            }
            else
            {
                state = CLI_STATE_IDLE;
            }

            break;


        case CLI_STATE_CMD:

            if (c == '<')
            {
                cmd_end = p;
                state = CLI_STATE_CMD_END1;
            }

            break;


        case CLI_STATE_CMD_END1:

            if (c == '<')
            {
                state = CLI_STATE_ARG_START;
            }
            else
            {
                state = CLI_STATE_IDLE;
            }

            break;


        case CLI_STATE_ARG_START:

            if (c == '{')
            {
                arg_begin = p + 1;
                state = CLI_STATE_ARG;
            }
            else
            {
                state = CLI_STATE_IDLE;
            }

            break;


        case CLI_STATE_ARG:

            if (c == '}')
            {
                arg_end = p;

                CLI_ParseCommand(cli,
                                 cmd_begin,
                                 cmd_end,
                                 arg_begin,
                                 arg_end);

                state = CLI_STATE_IDLE;
            }

            break;

        default:
            state = CLI_STATE_IDLE;
            break;
        }

        p++;
    }
}



void CLI_Input(cli_t *cli,
               const char *data,
               size_t len)
{
    if (!cli || !data)
        return;

    if (len + cli->index >= CLI_BUFFER_SIZE)
    {
        cli->index = 0;
    }

    memcpy(&cli->buffer[cli->index], data, len);
    cli->index += len;

    cli->buffer[cli->index] = '\0';

    CLI_ParseLine(cli, cli->buffer);

    cli->index = 0;
}