#include "relay.h"

/* 全局状态和阈值变量 */
uint8_t g_pumpState   = PUMP_OFF;
uint8_t g_pumpTempOn  = RELAY_TEMP_ON;
uint8_t g_pumpTempOff = RELAY_TEMP_OFF;
uint8_t g_pumpHumOn   = RELAY_HUM_ON;
uint8_t g_pumpHumOff  = RELAY_HUM_OFF;

void Relay_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = RELAY_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RELAY_PORT, &GPIO_InitStructure);

    Relay_SetOff();
}

void Relay_SetOn(void)
{
    GPIO_SetBits(RELAY_PORT, RELAY_PIN);
    g_pumpState = PUMP_ON;
}

void Relay_SetOff(void)
{
    GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
    g_pumpState = PUMP_OFF;
}

/*
 * Relay_Control：综合水泵控制逻辑
 *
 * 优先级：
 *   1. 有雨 → 强制关泵（最高优先级）
 *   2. 当前关闭状态：气温>温度上限 OR 湿度<湿度下限 → 开泵
 *   3. 当前开启状态：气温<温度下限 AND 湿度>湿度上限 → 关泵
 *   4. 其他情况保持当前状态（滞回防抖动）
 */
void Relay_Control(uint8_t airTemp, uint8_t soilHum, uint8_t hasRain)
{
    /* 优先级最高：检测到雨水立即关泵 */
    if (hasRain)
    {
        Relay_SetOff();
        return;
    }

    if (g_pumpState == PUMP_OFF)
    {
        /* 当前关闭：任一条件满足则开泵 */
        if (airTemp >= g_pumpTempOn || soilHum <= g_pumpHumOn)
            Relay_SetOn();
    }
    else
    {
        /* 当前开启：两个条件同时满足才关泵（无雨已在上方判断） */
        if (airTemp <= g_pumpTempOff && soilHum >= g_pumpHumOff)
            Relay_SetOff();
    }
}