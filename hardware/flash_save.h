#ifndef __FLASH_SAVE_H
#define __FLASH_SAVE_H

#include "stm32f10x.h"

// 声明存储和读取函数
void Flash_Save_GDD(float gdd);
void Flash_Load_GDD(float *gdd);

#endif