#include "rtc.h"

void RTC_Configuration(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5) {
        RCC_LSEConfig(RCC_LSE_ON);
        while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        RCC_RTCCLKCmd(ENABLE);
        RTC_WaitForSynchro();
        RTC_SetPrescaler(32767);
        RTC_WaitForLastTask();
        BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
    }
}

void Get_Time(MyTime *time) {
    uint32_t s = RTC_GetCounter();
    time->day = s / 2;
    time->hour = (s % 86400) / 3600;
    time->min = (s % 3600) / 60;
    time->sec = s % 60;
}
void RTC_ResetCounter(void)
{
    RTC_SetCounter(0);
    RTC_WaitForLastTask();
}