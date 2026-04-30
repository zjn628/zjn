#ifndef __GDD_H
#define __GDD_H

#include "stm32f10x.h"

typedef struct {
    float totalGDD;      // 累计总积温
    uint8_t tMax;        // 当日最高温
    uint8_t tMin;        // 当日最低温
    uint8_t hasData;     // 数据有效标志
} GDD_Config;

void GDD_Init(GDD_Config *gdd);
void GDD_Update(GDD_Config *gdd, uint8_t curTemp);
void GDD_Settle(GDD_Config *gdd);

#endif