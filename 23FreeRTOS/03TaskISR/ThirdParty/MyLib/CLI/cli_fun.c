#include "cli_fun.h"
#include "../../ThirdParty/MyLib/LED/LED.h"
#include "../../ThirdParty/MyLib/LCD/lcd.h"
#include "stm32f4xx_hal_def.h"
#include "usart.h"
#include "rtc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 安全的字符串严格分割函数 (支持连续分隔符/保留空参数)
 * * @param in_str        [入参] 原始常量字符串 (例如 "LED_ON,100,50")
 * @param delim         [入参] 分隔符集合，可以是多个字符,如传入", \n\r"是按照逗号、空格或者换行符分割
 * @param work_buf      [入参] 用户提供的局部缓冲区 (用于拷贝和切分)
 * @param work_buf_size [入参] 局部缓冲区的总大小
 * @param argv          [出参] 用于存放切分后子字符串指针的数组
 * @param max_argc      [入参] argv 数组的最大容量
 * * @return int          成功分割出的参数个数 (argc)
 */
int CLI_SplitArgs(const char *in_str, const char *delim, 
                  char *work_buf, size_t work_buf_size, 
                  char *argv[], int max_argc) 
{
    // 参数合法性防呆检查
    if (!in_str || !delim || !work_buf || !argv || max_argc <= 0 || work_buf_size == 0) {
        return 0;
    }

    // 1. 将只读的常量字符串安全地拷贝到局部缓冲区中
    strncpy(work_buf, in_str, work_buf_size - 1);
    work_buf[work_buf_size - 1] = '\0'; // 确保必定有字符串结束符

    // 如果本身就是空字符串，直接返回 0 个参数
    if (work_buf[0] == '\0') {
        return 0;
    }

    int argc = 0;
    char *p = work_buf;
    char *token_start = work_buf; // 记录当前参数的起始位置

    // 2. 遍历缓冲区，遇到分隔符就一刀切断
    while (*p != '\0') 
    {
        // 检查当前字符是否属于分隔符集合
        if (strchr(delim, *p) != NULL) 
        {
            *p = '\0';                  // 将分隔符替换为 '\0'，截断当前参数
            argv[argc++] = token_start; // 保存当前参数的指针
            
            if (argc >= max_argc) {
                return argc;            // 达到数组容量上限，安全退出
            }
            
            p++;
            token_start = p;            // 下一个参数的起点紧跟在当前分隔符之后
        } 
        else 
        {
            p++;                        // 不是分隔符，继续往后看
        }
    }

    // 3. 循环结束时，保存最后一个参数（即使它是空字符串）
    if (argc < max_argc) 
    {
        argv[argc++] = token_start;
    }

    return argc;
}

/*
 * @brief CLI 命令处理函数 - 控制 LED
 * @param cmd  [入参] 命令字符串，未使用
 * @param args [入参] 参数字符串，格式为 "LEDx,state",前者指定被控对象，后者指定状态
 * @note 当参数不符合预期时，被控对象默认是LEDG，状态默认为开启。
 * @note 命令示例：
 *   @CMD LED
 *   @ARG
 *      R,ON
 *   @END
 */
void cli_led(const char *cmd, const char *args){
    UNUSED(cmd);
    char work_buf[64];
    char *argv[2]; // 预设最多两个参数：LED编号和状态
    CLI_SplitArgs(args, ",", work_buf, sizeof(work_buf), argv, 2);
    void (*led_func)(uint8_t) = LEDG;
    printf("LED command received: LEDx:%s,state:%s\r\n", argv[0], argv[1]);
    // 解析 LED 编号 (假设格式为 "LEDx"，x 为颜色代码)
    if(argv[0][0] == 'R'||argv[0][3]=='R')led_func = LEDR;
    if(argv[0][0] == 'G'||argv[0][3]=='G')led_func = LEDG;

    if(argv[1][0] != '0'&&argv[1][1] != 'F')led_func(1);
    else led_func(0);
    
}



void cli_rtc(const char *cmd, const char *args){
    char RTCbuffer[128];
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    int year, month, date, hour, min, sec;
    // 尝试匹配 "年 月 日 时 分 秒" 格式
    if (sscanf((char *)args, "%d %d %d %d %d %d", 
                &year, &month, &date, &hour, &min, &sec) == 6)
    {
        // 如果成功匹配到 6 个参数，说明格式正确，调用设时函数
        Update_RTC_Time(year, month, date, hour, min, sec);
        printf("RTC Time Synced Successfully!\r\n");
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        sprintf(RTCbuffer, "Time: %02d:%02d:%02d,\nDate: 20%02d-%02d-%02d", 
            sTime.Hours, sTime.Minutes, sTime.Seconds,
            sDate.Year, sDate.Month, sDate.Date);
        printf("%s\n", RTCbuffer);

    }
    else
    {
        // 格式不匹配，输出错误提示
        printf("%s : Unknown Format: %s\r\n", cmd, args);
    }
}



void cli_lcd(const char *cmd, const char *args){
    UNUSED(cmd);
    UNUSED(args);
    printf("LCD command not supported now!\r\n");
}