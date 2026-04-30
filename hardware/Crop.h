#ifndef __CROP_H
#define __CROP_H

#include "stm32f10x.h"

#define CROP_STAGE_NUM  4
#define CROP_WHEAT      0
#define CROP_CORN       1
#define CROP_NUM        2

typedef struct {
    char     name[8];
    char     nameCN[8];
    uint8_t  baseTemp;
    uint16_t stageGDD[CROP_STAGE_NUM];      // 运行时可修改（非const）
    char     stageName[CROP_STAGE_NUM][10];
} CropPreset;

extern uint8_t    g_cropIndex;
extern CropPreset g_cropPresets[CROP_NUM];  // 去掉const，允许设置界面修改阈值

uint8_t Crop_GetStage(float totalGDD);
uint8_t Crop_GetProgress(float totalGDD);
uint8_t Crop_GetBaseTemp(void);

#endif