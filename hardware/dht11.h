#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

typedef struct
{
    uint8_t temperature;	// 存储读取到的温度值
    uint8_t humidity;		// 存储读取到的湿度值
} DHT11_DataTypedef;

void DHT11_Init(void);
uint8_t DHT11_Read_Data(DHT11_DataTypedef *data);

#endif