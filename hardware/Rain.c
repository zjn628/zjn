#include "rain.h"

/*
 * Rain_Init
 * 功能：初始化PB15为内部上拉输入
 *       传感器未检测到雨水时DO口悬空，内部上拉保证读到高电平
 *       检测到雨水时DO口输出低电平
 */
void Rain_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin  = RAIN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 内部上拉
    GPIO_Init(RAIN_PORT, &GPIO_InitStructure);
}

/*
 * Rain_Detected
 * 功能：读取DO口状态
 * 返回：1 = 检测到雨水（低电平）
 *        0 = 无雨（高电平）
 */
uint8_t Rain_Detected(void)
{
    return (GPIO_ReadInputDataBit(RAIN_PORT, RAIN_PIN) == 0) ? 1 : 0;
}