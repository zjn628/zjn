#ifndef __AIR780E_H
#define __AIR780E_H

#include "stm32f10x.h"

/* 缓冲区大小定义 */
#define RX_BUF_SIZE 512

/* 外部变量声明，供 main.c 或其他文件使用 */
extern char rxBuffer[RX_BUF_SIZE];
extern uint16_t rxIndex;

/* 基础驱动函数 */
void USART2_Init(void);
void USART2_SendString(char *str);
char* USART2_WaitResponse(char *expect, uint32_t timeout);
void ClearRxBuffer(void);

/* 业务逻辑函数 */
uint8_t Air780E_Init(void);
uint8_t Air780E_LuatCloudConnect(char *projectKey);
uint8_t Air780E_LuatCloudPublish(char *jsonData);

#endif