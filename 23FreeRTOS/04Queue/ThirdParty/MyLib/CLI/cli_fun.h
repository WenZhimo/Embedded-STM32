#ifndef __CLI_FUN_H__
#define __CLI_FUN_H__

#include "cli.h"



void cli_led(const char *cmd, const char *args);
void cli_rtc(const char *cmd, const char *args);
void cli_lcd(const char *cmd, const char *args);

//工具函数：安全的字符串分割函数，将输入字符串按照指定的分隔符切割成多个参数，存储在 argv 数组中，并返回参数数量。
int CLI_SplitArgs(const char *in_str, const char *delim, 
                  char *work_buf, size_t work_buf_size, 
                  char *argv[], int max_argc);



#endif

