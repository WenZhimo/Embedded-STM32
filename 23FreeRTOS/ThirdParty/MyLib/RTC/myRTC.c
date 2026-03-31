#include "../../ThirdParty/MyLib/myinclude.h"

_calendar_obj calendar; /* 时钟结构体实例化 */

/* ======================================================================= */
/* ======================== 1. 基础功能及转换算法 ======================== */
/* =================== (两套模式公用，无论何种芯片都会编译) ================= */
/* ======================================================================= */

void myRTC_Write_BKR(uint32_t bkrx, uint16_t data)
{
    HAL_PWR_EnableBkUpAccess(); /* 取消备份区写保护 */
    HAL_RTCEx_BKUPWrite(&hrtc, bkrx + 1, data);
}

uint16_t myRTC_Read_BKR(uint32_t bkrx)
{
    return (uint16_t)HAL_RTCEx_BKUPRead(&hrtc, bkrx + 1);
}

uint8_t myRTC_Is_Leap_Year(uint16_t year)
{
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 1;
    else return 0;
}

uint8_t myRTC_Get_Week(uint16_t year, uint8_t month, uint8_t day)
{
    uint8_t week = 0;
    if (month < 3) { month += 12; --year; }
    week = (day + 1 + 2 * month + 3 * (month + 1) / 5 + year + (year >> 2) - year / 100 + year / 400) % 7;
    return week;
}

long myRTC_DateToSec(uint16_t syear, uint8_t smon, uint8_t sday, uint8_t hour, uint8_t min, uint8_t sec)
{
    uint32_t Y, M, D, X, T;
    signed char monx = smon;
    if (0 >= (monx -= 2)) { monx += 12; syear -= 1; }
    Y = (syear - 1) * 365 + syear / 4 - syear / 100 + syear / 400;
    M = 367 * monx / 12 - 30 + 59;
    D = sday - 1;
    X = Y + M + D - 719162;
    T = ((X * 24 + hour) * 60 + min) * 60 + sec;
    return T;
}

/* ======================================================================= */
/* ========================= 2. 差异化硬件驱动代码 ========================= */
/* ======================================================================= */

#if RTC_CHIP_MODE == 1
/* ----------------------------------------------------------------------- */
/* ------------------- [模式1] 硬件日历模式 (F4/G4/H7等) ------------------- */
/* ----------------------------------------------------------------------- */

uint8_t myRTC_Set_Time(uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    uint8_t hw_year = year % 100; // 提取后两位
    if(month < 1 || month > 12 || date < 1 || date > 31 || hour > 23 || min > 59 || sec > 59) return 1;

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    // 写入时间
    sTime.Hours = hour;
    sTime.Minutes = min;
    sTime.Seconds = sec;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    
    // 写入日期
    sDate.Year = hw_year;
    sDate.Month = month;
    sDate.Date = date;
    sDate.WeekDay = myRTC_Get_Week(year, month, date);
    if(sDate.WeekDay == 0) sDate.WeekDay = RTC_WEEKDAY_SUNDAY; 
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x32F4);
    HAL_PWR_DisableBkUpAccess();
    
    myRTC_Get_Time(); // 同步一次全局变量
    return 0;
}

void myRTC_Get_Time(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // F4等高级日历必须先读 Time，再读 Date！
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    calendar.hour  = sTime.Hours;
    calendar.min   = sTime.Minutes;
    calendar.sec   = sTime.Seconds;
    calendar.year  = sDate.Year + 2000;
    calendar.month = sDate.Month;
    calendar.date  = sDate.Date;
    calendar.week  = sDate.WeekDay;
    if(calendar.week == RTC_WEEKDAY_SUNDAY) calendar.week = 0; 
}

uint8_t myRTC_Set_Alarm(uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec)
{
    // 这里预留F4/G4的闹钟配置代码。因为高级RTC使用 RTC_AlarmTypeDef 结构体，
    // 通常只设置 天、时、分、秒 的匹配掩码。由于篇幅和各芯片HAL库结构体微小差异，
    // 可在后续具体使用到高级闹钟时填入 HAL_RTC_SetAlarm_IT() 逻辑。
    return 0;
}


#elif RTC_CHIP_MODE == 2
/* ----------------------------------------------------------------------- */
/* ------------------- [模式2] 秒计数器模式 (F103 等型号) ------------------- */
/* ----------------------------------------------------------------------- */

uint8_t myRTC_Set_Time(uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec)
{
    uint32_t seccount = myRTC_DateToSec(year, month, date, hour, min, sec);

    __HAL_RCC_PWR_CLK_ENABLE(); 
    __HAL_RCC_BKP_CLK_ENABLE(); 
    HAL_PWR_EnableBkUpAccess(); 
    
    RTC->CRL |= 1 << 4;                 /* 进入配置模式 */
    RTC->CNTL = seccount & 0xffff;      /* 设置低16位 */
    RTC->CNTH = seccount >> 16;         /* 设置高16位 */
    RTC->CRL &= ~(1 << 4);              /* 退出配置 */

    while (!__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_RTOFF)); /* 等待操作完成 */
    return 0;
}

void myRTC_Get_Time(void)
{
    static uint16_t daycnt = 0;
    uint32_t seccount = 0;
    uint32_t temp = 0;
    uint16_t temp1 = 0;
    const uint8_t month_table = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; 

    seccount = RTC->CNTH;       
    seccount <<= 16;
    seccount += RTC->CNTL;

    temp = seccount / 86400;    /* 得到天数 */
    if (daycnt != temp)         
    {
        daycnt = temp;
        temp1 = 1970;           

        while (temp >= 365)
        {
            if (myRTC_Is_Leap_Year(temp1)) { if (temp >= 366) temp -= 366; else break; }
            else temp -= 365;    
            temp1++;
        }
        calendar.year = temp1;  
        temp1 = 0;

        while (temp >= 28)      
        {
            if (myRTC_Is_Leap_Year(calendar.year) && temp1 == 1) { if (temp >= 29) temp -= 29; else break; }
            else { if (temp >= month_table[temp1]) temp -= month_table[temp1]; else break; }
            temp1++;
        }
        calendar.month = temp1 + 1; 
        calendar.date = temp + 1;   
    }

    temp = seccount % 86400;                                                    
    calendar.hour = temp / 3600;                                                
    calendar.min = (temp % 3600) / 60;                                          
    calendar.sec = (temp % 3600) % 60;                                          
    calendar.week = myRTC_Get_Week(calendar.year, calendar.month, calendar.date); 
}

uint8_t myRTC_Set_Alarm(uint16_t year, uint8_t month, uint8_t date, uint8_t hour, uint8_t min, uint8_t sec)
{
    uint32_t seccount = myRTC_DateToSec(year, month, date, hour, min, sec); 

    __HAL_RCC_PWR_CLK_ENABLE(); 
    __HAL_RCC_BKP_CLK_ENABLE(); 
    HAL_PWR_EnableBkUpAccess(); 
    
    RTC->CRL |= 1 << 4;         
    RTC->ALRL = seccount & 0xffff;
    RTC->ALRH = seccount >> 16;
    RTC->CRL &= ~(1 << 4);      

    while (!__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_RTOFF)); 
    return 0;
}

#endif