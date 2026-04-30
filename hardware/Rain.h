#ifndef __RAIN_H
#define __RAIN_H

#include "stm32f10x.h"

/* 雨水传感器DO口接PB15，低电平=检测到雨水 */
#define RAIN_PORT   GPIOB
#define RAIN_PIN    GPIO_Pin_15

void    Rain_Init(void);
uint8_t Rain_Detected(void);  // 返回1=有雨，0=无雨

#endif