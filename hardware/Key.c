#include "key.h"

/*
 * Key_Init：初始化按键引脚，全部内部上拉，按下为低电平
 * K1 MODE  → PB14
 * K2 UP    → PA9
 * K3 DOWN  → PA10
 * K4 OK    → PB13
 */
void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;

    /* PA9 PA10：UP / DOWN */
    GPIO_InitStructure.GPIO_Pin = KEY_UP_PIN | KEY_DOWN_PIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PB13 PB14：OK / MODE */
    GPIO_InitStructure.GPIO_Pin = KEY_OK_PIN | KEY_MODE_PIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/*
 * Key_Scan：轮询扫描，调用周期10ms
 * 消抖：连续2次检测到低电平才确认（20ms）
 * 长按：OK键持续约1秒触发 KEY_OK_LONG
 */
uint8_t Key_Scan(void)
{
    static uint8_t  lastKey     = KEY_NONE;
    static uint8_t  debounce    = 0;
    static uint16_t okHoldCnt   = 0;
    static uint8_t  okLongFired = 0;

    uint8_t cur = KEY_NONE;

    if      (!GPIO_ReadInputDataBit(KEY_PORT_MODE,  KEY_MODE_PIN))  cur = KEY_MODE;
    else if (!GPIO_ReadInputDataBit(KEY_PORT_UP,    KEY_UP_PIN))    cur = KEY_UP;
    else if (!GPIO_ReadInputDataBit(KEY_PORT_DOWN,  KEY_DOWN_PIN))  cur = KEY_DOWN;
    else if (!GPIO_ReadInputDataBit(KEY_PORT_OK,    KEY_OK_PIN))    cur = KEY_OK;

    /* OK 长按计时 */
    if (cur == KEY_OK)
    {
        okHoldCnt++;
        if (okHoldCnt >= 100 && !okLongFired)
        {
            okLongFired = 1;
            okHoldCnt   = 0;
            lastKey     = KEY_NONE;
            debounce    = 0;
            return KEY_OK_LONG;
        }
    }
    else
    {
        okHoldCnt   = 0;
        okLongFired = 0;
    }

    /* 消抖 + 边沿检测，只在按下瞬间触发一次 */
    if (cur != KEY_NONE && cur == lastKey)
    {
        debounce++;
        if (debounce == 2) return cur;
        if (debounce >  2) debounce = 3;
    }
    else
    {
        lastKey  = cur;
        debounce = 0;
    }

    return KEY_NONE;
}