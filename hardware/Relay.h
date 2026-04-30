#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f10x.h"

/* 继电器控制引脚：PA12 */
#define RELAY_PORT      GPIOA
#define RELAY_PIN       GPIO_Pin_12

/* 温度默认滞回阈值 */
#define RELAY_TEMP_ON   30
#define RELAY_TEMP_OFF  28

/* 湿度默认滞回阈值（湿度低于ON阈值开泵，高于OFF阈值关泵） */
#define RELAY_HUM_ON    30      // 湿度低于30%开泵
#define RELAY_HUM_OFF   40      // 湿度高于40%关泵

/* 水泵状态 */
#define PUMP_OFF        0
#define PUMP_ON         1

/* 对外暴露的状态和阈值变量 */
extern uint8_t g_pumpState;
extern uint8_t g_pumpTempOn;    // 温度开泵阈值
extern uint8_t g_pumpTempOff;   // 温度关泵阈值
extern uint8_t g_pumpHumOn;     // 湿度开泵阈值（低于此值开泵）
extern uint8_t g_pumpHumOff;    // 湿度关泵阈值（高于此值关泵）

/* 函数声明 */
void Relay_Init(void);
void Relay_SetOn(void);
void Relay_SetOff(void);

/*
 * Relay_Control：综合控制函数
 * 参数：airTemp  - 当前气温
 *        soilHum  - 当前土壤湿度百分比
 *        hasRain  - 雨水检测结果（1=有雨）
 *
 * 开泵逻辑（任一满足）：
 *   气温 > g_pumpTempOn  OR  土壤湿度 < g_pumpHumOn
 *
 * 关泵逻辑（雨水优先，其次同时满足）：
 *   有雨 → 立即关泵
 *   气温 < g_pumpTempOff AND 土壤湿度 > g_pumpHumOff → 关泵
 */
void Relay_Control(uint8_t airTemp, uint8_t soilHum, uint8_t hasRain);

#endif
