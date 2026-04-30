#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

/* 按键引脚定义 */
#define KEY_MODE_PIN    GPIO_Pin_14  // K1: PB14
#define KEY_UP_PIN      GPIO_Pin_9   // K2: PA9
#define KEY_DOWN_PIN    GPIO_Pin_10  // K3: PA10
#define KEY_OK_PIN      GPIO_Pin_13  // K4: PB13

#define KEY_PORT_MODE   GPIOB        // K1 PB14
#define KEY_PORT_UP     GPIOA        // K2 PA9
#define KEY_PORT_DOWN   GPIOA        // K3 PA10
#define KEY_PORT_OK     GPIOB        // K4 PB13

/* 返回值定义 */
#define KEY_NONE        0
#define KEY_MODE        1
#define KEY_UP          2
#define KEY_DOWN        3
#define KEY_OK          4
#define KEY_OK_LONG     5

void    Key_Init(void);
uint8_t Key_Scan(void);

#endif