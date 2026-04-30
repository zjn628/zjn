#include "at.h"
#include "stm32f10x_bkp.h"

void GDD_Save(GDD_Config *gdd)
{
    BKP_WriteBackupRegister(BKP_DR3, (uint16_t)gdd->totalGDD);
}

void GDD_Load(GDD_Config *gdd)
{
    gdd->totalGDD = BKP_ReadBackupRegister(BKP_DR3);
}

void GDD_Init(GDD_Config *gdd) {
    gdd->totalGDD = 0.0f;
    gdd->tMax = 0;
    gdd->tMin = 255;
    gdd->hasData = 0;
}

void GDD_Update(GDD_Config *gdd, uint8_t curTemp) {
    if (!gdd->hasData) {
        gdd->tMax = gdd->tMin = curTemp;
        gdd->hasData = 1;
    } else {
        if (curTemp > gdd->tMax) gdd->tMax = curTemp;
        if (curTemp < gdd->tMin) gdd->tMin = curTemp;
    }
}

void GDD_Settle(GDD_Config *gdd) {
    if (gdd->hasData) {
        float daily = ((float)gdd->tMax + (float)gdd->tMin) / 2.0f - 10.0f;
        if (daily > 0) gdd->totalGDD += daily;
        // 重置当日数据
        gdd->tMax = 0; gdd->tMin = 255; gdd->hasData = 0;
    }
}