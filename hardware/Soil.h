#ifndef __SOIL_H
#define __SOIL_H

#include "stm32f10x.h"

/* 土壤湿度传感器AO接PA1，对应ADC1通道1 */
#define SOIL_ADC_CHANNEL    ADC_Channel_1
#define SOIL_GPIO_PORT      GPIOA
#define SOIL_GPIO_PIN       GPIO_Pin_1

/*
 * 标定值：实测ADC值
 * ADC_DRY：传感器插在干燥空气中的ADC值（约3500）
 * ADC_WET：传感器插在水中的ADC值（约1200）
 * 实际使用前建议用自己的传感器重新标定这两个值
 */
#define SOIL_ADC_DRY        4095
#define SOIL_ADC_WET        1162

/* 平均滤波采样次数 */
#define SOIL_SAMPLE_COUNT   10

/* 函数声明 */
void    Soil_Init(void);
uint16_t Soil_ReadADC(void);
uint16_t Soil_ReadADC_Avg(void);
uint8_t  Soil_GetHumidity(void);

#endif