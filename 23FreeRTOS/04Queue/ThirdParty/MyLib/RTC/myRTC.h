#ifndef __MYRTC_H
#define __MYRTC_H

#include "main.h"  // 确保包含HAL库头文件
#include "rtc.h"


/* =================================================================================
 * [核心配置] 芯片RTC模式选择
 * 请根据你的芯片型号修改此宏：
 * 1 : 硬件日历模式 (适用于 STM32F4 / F7 / G0 / G4 / H7 / L4 等带有高级RTC的型号)
 * 2 : 秒计数器模式 (适用于 STM32F103 等基础RTC型号)
 * ================================================================================= */
#define RTC_CHIP_MODE  1  


/* 时间结构体, 包括年月日周时分秒等信息 */
typedef struct
{
    uint8_t hour;       /* 时 */
    uint8_t min;        /* 分 */
    uint8_t sec;        /* 秒 */
    uint16_t year;      /* 年 (4位数, 如 2025) */
    uint8_t  month;     /* 月 */
    uint8_t  date;      /* 日 */
    uint8_t  week;      /* 周 */
} _calendar_obj;

extern _calendar_obj calendar;      /* 供外部引用的时间结构体 */
extern RTC_HandleTypeDef hrtc;      /* 声明外部的RTC句柄 */


/* ================== 通用功能性函数 ================== */
void myRTC_Write_BKR(uint32_t bkrx, uint16_t data);                 /* 写后备寄存器 */
uint16_t myRTC_Read_BKR(uint32_t bkrx);                             /* 读取后备寄存器 */
uint8_t myRTC_Get_Week(uint16_t year, uint8_t month, uint8_t day);  /* 根据年月日获取星期几 */
uint8_t myRTC_Is_Leap_Year(uint16_t year);                          /* 判断是否是闰年 */
long myRTC_DateToSec(uint16_t syear, uint8_t smon, uint8_t sday, uint8_t hour, uint8_t min, uint8_t sec);

/* ================== 统一的操作接口API ================== */
/* 无论底层是哪种芯片，应用层只管调这三个接口就行了 */
uint8_t myRTC_Set_Time(uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec);
void myRTC_Get_Time(void);
uint8_t myRTC_Set_Alarm(uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec);

#endif // __MYRTC_H