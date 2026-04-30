#ifndef __RTC_H
#define __RTC_H

#include "stm32f10x.h"

typedef struct {
    uint32_t day;
    uint8_t hour, min, sec;
} MyTime;

void RTC_Configuration(void);
void Get_Time(MyTime *time);

#endif