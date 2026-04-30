#include "ds18b20.h"

// 引脚定义：PA0
#define DS18B20_PORT GPIOA
#define DS18B20_PIN  GPIO_Pin_0

// 使用 TIM1 实现精准微秒延时 (复用你的工程逻辑)
static void Delay_us(uint16_t us)
{
    TIM_SetCounter(TIM1, 0);
    while (TIM_GetCounter(TIM1) < us);
}

// 设置 PA0 为推挽输出
static void DS18B20_Mode_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = DS18B20_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_PORT, &GPIO_InitStructure);
}

// 设置 PA0 为浮空输入
static void DS18B20_Mode_In(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = DS18B20_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DS18B20_PORT, &GPIO_InitStructure);
}

// DS18B20 复位与存在检测
uint8_t DS18B20_Reset(void)
{
    uint8_t response = 0;
    DS18B20_Mode_Out();
    GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN); // 拉低总线
    Delay_us(500);                             // 保持 500us
    GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);    // 释放总线
    Delay_us(70);                              // 等待 70us
    
    DS18B20_Mode_In();
    if(GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN) == 0) response = 1;
    Delay_us(430); 
    return response;
}

// 向 DS18B20 写一个字节
void DS18B20_WriteByte(uint8_t dat)
{
    uint8_t i;
    DS18B20_Mode_Out();
    for (i = 0; i < 8; i++)
    {
        GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
        Delay_us(2);
        if (dat & 0x01) GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
        else            GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
        Delay_us(60);
        GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
        dat >>= 1;
    }
}

// 从 DS18B20 读一个字节
uint8_t DS18B20_ReadByte(void)
{
    uint8_t i, dat = 0;
    for (i = 0; i < 8; i++)
    {
        DS18B20_Mode_Out();
        GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
        Delay_us(2);
        GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
        DS18B20_Mode_In();
        Delay_us(12);
        dat >>= 1;
        if (GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN)) dat |= 0x80;
        Delay_us(50);
    }
    return dat;
}

void DS18B20_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DS18B20_Reset();
}

float DS18B20_GetTemp(void)
{
    uint8_t TL, TH;
    short temp;
    if (DS18B20_Reset())
    {
        DS18B20_WriteByte(0xCC); // 跳过 ROM
        DS18B20_WriteByte(0x44); // 开始温度转换
        
        DS18B20_Reset();
        DS18B20_WriteByte(0xCC);
        DS18B20_WriteByte(0xBE); // 读暂存器
        
        TL = DS18B20_ReadByte(); 
        TH = DS18B20_ReadByte(); 
        temp = (TH << 8) | TL;
        return (float)temp * 0.0625f;
    }
    return -99.9f;
}