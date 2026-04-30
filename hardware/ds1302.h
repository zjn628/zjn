#ifndef __DS1302_H
#define __DS1302_H

#include "stm32f10x.h"

typedef struct {
    uint8_t year;   
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t week;
} DS1302_Time;

void DS1302_Init(void);
void DS1302_ReadTime(DS1302_Time *time);
void DS1302_SetTime(DS1302_Time *time); // 确保这一行存在

#endif