#ifndef SDLOG_H
#define SDLOG_H

#include "ds1302.h"

void SDLog_Init(void);
void SDLog_Write(uint8_t airTemp, float soilTemp, uint8_t soilHum, float totalGDD, uint8_t hasRain, uint8_t pumpState, DS1302_Time *time);
void SDLog_NewDay(void);

#endif