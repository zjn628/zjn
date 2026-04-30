#include "crop.h"

uint8_t g_cropIndex = CROP_WHEAT;

/* 去掉const，允许设置界面直接修改 stageGDD[] */
CropPreset g_cropPresets[CROP_NUM] = {
    /* CROP_WHEAT - 教材标准，基准10°C */
    {
        .name      = "Wheat",
        .nameCN    = "Wheat",
        .baseTemp  = 10,
        .stageGDD  = {150, 400, 800, 1200},
        .stageName = {"Germinate", "Tillering", "Jointing", "Maturity"}
    },
    /* CROP_CORN - 教材标准，基准10°C */
    {
        .name      = "Corn",
        .nameCN    = "Corn",
        .baseTemp  = 10,
        .stageGDD  = {100, 400, 700, 1100},
        .stageName = {"Germinate", "Jointing", "Heading", "Maturity"}
    }
};

uint8_t Crop_GetStage(float totalGDD)
{
    const CropPreset *p = &g_cropPresets[g_cropIndex];
    for (uint8_t i = 0; i < CROP_STAGE_NUM - 1; i++)
        if ((uint16_t)totalGDD < p->stageGDD[i]) return i;
    return CROP_STAGE_NUM - 1;
}

uint8_t Crop_GetProgress(float totalGDD)
{
    const CropPreset *p       = &g_cropPresets[g_cropIndex];
    uint8_t           st      = Crop_GetStage(totalGDD);
    uint16_t          stStart = (st == 0) ? 0 : p->stageGDD[st - 1];
    uint16_t          stEnd   = p->stageGDD[st];
    uint16_t          range   = stEnd - stStart;
    if (range == 0) return 100;
    uint16_t cur = (uint16_t)totalGDD;
    if (cur <= stStart) return 0;
    if (cur >= stEnd)   return 100;
    return (uint8_t)(((uint32_t)(cur - stStart) * 100) / range);
}

uint8_t Crop_GetBaseTemp(void)
{
    return g_cropPresets[g_cropIndex].baseTemp;
}