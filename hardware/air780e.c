#include "air780e.h"
#include <stdio.h>
#include <string.h>

/* 全局变量 */
char rxBuffer[RX_BUF_SIZE];
uint16_t rxIndex = 0;
extern volatile uint32_t g_ms; // 引用 main.c 中的毫秒计数器

/* 1. USART2 初始化 (标准库版本) */
void USART2_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 开启 PA 和 USART2 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA2 (TX) 复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA3 (RX) 浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* USART2 配置: 115200 8-N-1 */
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    /* 开启串口接收中断 */
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    /* 配置 NVIC 中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

/* 2. 发送字符串 */
void USART2_SendString(char *str) {
    while (*str) {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, (uint16_t)*str++);
    }
}

/* 3. 清空接收缓冲区 */
void ClearRxBuffer(void) {
    memset(rxBuffer, 0, RX_BUF_SIZE);
    rxIndex = 0;
}

/* 4. 等待响应 (带超时机制) */
char* USART2_WaitResponse(char *expect, uint32_t timeout) {
    uint32_t startTime = g_ms;
    while (g_ms - startTime < timeout) {
        if (strstr(rxBuffer, expect)) {
            return strstr(rxBuffer, expect);
        }
    }
    return NULL;
}

/* 5. 模块基础初始化 */
uint8_t Air780E_Init(void) {
    uint8_t retry = 0;
    for (retry = 0; retry < 5; retry++) {
        ClearRxBuffer();
        USART2_SendString("AT\r\n");
        if (USART2_WaitResponse("OK", 1000)) return 1;
    }
    return 0;
}

/* 6. 连接合宙云 */
uint8_t Air780E_LuatCloudConnect(char *projectKey) {
    uint8_t retry;
    char cmd[128];

    // 1. 获取IMEI
    for (retry = 0; retry < 3; retry++) {
        ClearRxBuffer();
        USART2_SendString("AT+CGSN\r\n");
        if (USART2_WaitResponse("OK", 2000)) break;
        if (retry == 2) return 0;
    }

    // 2. MQTT配置
    sprintf(cmd, "AT+MQTTUSERCFG=0,1,\"Air780E\",\"%s\",\"%s\",0,0,\"\"\r\n", projectKey, projectKey);
    ClearRxBuffer();
    USART2_SendString(cmd);
    if (!USART2_WaitResponse("OK", 2000)) return 0;

    // 3. 打开MQTT连接
    ClearRxBuffer();
    USART2_SendString("AT+MQTTOPEN=0,\"mqtt.luatos.com\",1883\r\n");
    if (!USART2_WaitResponse("+MQTTOPEN: 0,0", 5000)) return 0;

    // 4. 建立MQTT会话
    ClearRxBuffer();
    USART2_SendString("AT+MQTTCONN=0,120,0,0\r\n");
    if (!USART2_WaitResponse("+MQTTCONN: 0,0", 5000)) return 0;

    return 1;
}

/* 7. 发布数据 */
uint8_t Air780E_LuatCloudPublish(char *jsonData) {
    char cmd[512];
    ClearRxBuffer();
    sprintf(cmd, "AT+MQTTPUB=0,\"/device/up\",\"%s\",0,0\r\n", jsonData);
    USART2_SendString(cmd);
    if (USART2_WaitResponse("OK", 3000)) return 1;
    return 0;
}

/* 8. USART2 中断服务函数 */
void USART2_IRQHandler(void) {
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint8_t data = (uint8_t)USART_ReceiveData(USART2);
        if (rxIndex < RX_BUF_SIZE - 1) {
            rxBuffer[rxIndex++] = data;
        }
    }
}