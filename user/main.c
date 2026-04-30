#include "stm32f10x.h"
#include "OLED.h"
#include "dht11.h"
#include "ds18b20.h"
#include "ds1302.h"
#include "key.h"
#include "crop.h"
#include "ui.h"
#include "relay.h"
#include "soil.h"
#include "rain.h"
#include "sdlog.h"
#include "air780e.h"  // 需确保 air780e.h 已在工程中
#include <stdio.h>

/* ===== 宏定义 ===== */
#define SAVE_ADDR_GDD   0x0800FC00
#define SAVE_ADDR_CROP  0x0800FC04
#define PROJECT_KEY     "eRF1tKOEgcqLN7CIKlsf7iXRjznlVkS4"  // 合宙云项目Key
#define UPLOAD_INTERVAL 300000        // 上传间隔：5分钟 (300000ms)

/* ===== 全局变量 ===== */
DS1302_Time      now;
float            totalGDD = 0.0f;
uint8_t          tMax = 0, tMin = 255, hasData = 0;
uint8_t          lastDay  = 0;
uint8_t          airTemp  = 0;
uint8_t          humidity = 0;
float            soilTemp = 0.0f;
uint8_t          soilHum  = 0;
uint8_t          hasRain  = 0;

volatile uint32_t g_ms = 0;             // 毫秒计数，用于 4G 上传计时
volatile uint8_t  g_tick10ms = 0;       // 按键 10ms 节拍
uint32_t          lastUploadTime = 0;   // 记录上次上传时间
static uint8_t    lastMin = 99;

/* ===== 中断服务函数 ===== */

/**
 * @brief SysTick 中断：1ms 触发一次
 */
void SysTick_Handler(void)
{
    g_ms++;
}

/**
 * @brief TIM2 中断：10ms 触发一次，用于按键扫描
 */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        g_tick10ms = 1;
    }
}

/* ===== Flash 存储辅助函数 ===== */

static void Flash_Save(float gdd, uint8_t cropIdx)
{
    uint32_t gddRaw = *(uint32_t*)&gdd;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    FLASH_ErasePage(SAVE_ADDR_GDD);
    FLASH_ProgramWord(SAVE_ADDR_GDD,  gddRaw);
    FLASH_ProgramWord(SAVE_ADDR_CROP, (uint32_t)cropIdx);
    FLASH_Lock();
}

static float Flash_LoadGDD(void)
{
    uint32_t raw = *(__IO uint32_t*)SAVE_ADDR_GDD;
    if (raw == 0xFFFFFFFF) return 0.0f;
    return *(float*)&raw;
}

static uint8_t Flash_LoadCrop(void)
{
    uint32_t raw = *(__IO uint32_t*)SAVE_ADDR_CROP;
    if (raw >= CROP_NUM) return 0;
    return (uint8_t)raw;
}

/* ===== 初始化辅助函数 ===== */

static void Timer1_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    TIM_TimeBaseInitTypeDef ts;
    ts.TIM_Period        = 0xFFFF;
    ts.TIM_Prescaler     = 71;
    ts.TIM_ClockDivision = 0;
    ts.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &ts);
    TIM_Cmd(TIM1, ENABLE);
}

static void Timer2_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseInitTypeDef ts;
    ts.TIM_Period        = 9999;
    ts.TIM_Prescaler     = 71;
    ts.TIM_ClockDivision = 0;
    ts.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &ts);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    NVIC_InitTypeDef n;
    n.NVIC_IRQChannel                   = TIM2_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 0;
    n.NVIC_IRQChannelSubPriority        = 1;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);
}

static void ReadTime_Safe(DS1302_Time *t)
{
    DS1302_Time temp;
    uint8_t pumpWasOn = g_pumpState;

    Relay_SetOff(); // 读时钟前关闭水泵，减少 EMI
    for (uint32_t i = 0; i < 7200; i++); // 简易延时
    DS1302_ReadTime(&temp);
    if (pumpWasOn == PUMP_ON) Relay_SetOn();

    if (temp.year <= 99 && temp.month >= 1 && temp.month <= 12)
        *t = temp;
}

/* ===== 主程序 ===== */
int main(void)
{
    /* 1. 硬件基础初始化 */
    SysTick_Config(SystemCoreClock / 1000); // 开启 1ms 滴答定时器
    Timer1_Init();
    Timer2_Init();
    Key_Init();
    OLED_Init();
    DS1302_Init();
    DHT11_Init();
    DS18B20_Init();
    Relay_Init();
    Soil_Init();    
    Rain_Init();
    USART2_Init();      // 初始化串口2用于 4G 模块

    /* 2. 掉电数据恢复 */
    totalGDD    = Flash_LoadGDD();
    g_cropIndex = Flash_LoadCrop();
    DS1302_ReadTime(&now);
    lastDay = now.day;
    lastMin = now.min;

    /* 3. 模块功能初始化 */
    UI_Init();
    SDLog_Init();
    
    /* 4. Air780E 联网 (尝试连接合宙云) */
    if (Air780E_Init()) {
        Air780E_LuatCloudConnect(PROJECT_KEY);
    }

    while (1)
    {
        /* ---- 10ms节拍：按键扫描 ---- */
        if (g_tick10ms)
        {
            g_tick10ms = 0;
            uint8_t key = Key_Scan();
            if (key != KEY_NONE)
            {
                uint8_t ret = UI_HandleKey(key, &now, &totalGDD);
                if (ret == 1) { // 切换作物
                    Flash_Save(totalGDD, g_cropIndex);
                    tMax = 0; tMin = 255; hasData = 0;
                } else if (ret == 2) { // 手动校时等
                    Flash_Save(totalGDD, g_cropIndex);
                }
            }
        }

        /* ---- 数据采集周期（每秒执行一次） ---- */
        if (!g_inSet) ReadTime_Safe(&now);

        static uint8_t lastSec = 99;
        if (now.sec != lastSec)
        {
            lastSec = now.sec;

            /* 传感器采集 */
            DHT11_DataTypedef dht;
            if (DHT11_Read_Data(&dht)) {
                airTemp  = dht.temperature;
                humidity = dht.humidity;
                uint8_t tEff = (airTemp > g_tempMax) ? g_tempMax : airTemp;
                if (!hasData) { tMax = tMin = tEff; hasData = 1; }
                else {
                    if (tEff > tMax) tMax = tEff;
                    if (tEff < tMin) tMin = tEff;
                }
            }

            float ds = DS18B20_GetTemp();
            if (ds > -50.0f) soilTemp = ds;
            soilHum = Soil_GetHumidity();
            hasRain = Rain_Detected();

            /* 水泵逻辑控制 */
            Relay_Control((uint8_t)soilTemp, soilHum, hasRain);

            /* 每分钟写入本地 SD 卡日志 */
            if (now.min != lastMin) {
                lastMin = now.min;
                SDLog_Write(airTemp, soilTemp, soilHum, totalGDD, hasRain, g_pumpState, &now);
            }
        }

        /* ---- 4G 云端数据上报逻辑（5分钟定时） ---- */
        if (g_ms - lastUploadTime >= UPLOAD_INTERVAL)
        {
            lastUploadTime = g_ms;

            char jsonData[256];
            /* 严格按要求格式化 JSON：pumpState 必须为最后一个字段 */
            sprintf(jsonData, 
                "{"
                "\"airTemp\":%.1f,"
                "\"humidity\":%.1f,"
                "\"soilTemp\":%.1f,"
                "\"soilHum\":%.1f,"
                "\"hasRain\":%d,"
                "\"totalGDD\":%.1f,"
                "\"pumpState\":%d"
                "}",
                (float)airTemp, (float)humidity, soilTemp, (float)soilHum, hasRain, totalGDD, g_pumpState);

            Air780E_LuatCloudPublish(jsonData);
        }

        /* ---- 跨天积温结算 ---- */
        if (!g_inSet && now.day != lastDay)
        {
            if (hasData) {
                float base  = (float)g_baseTemp;
                float daily = ((float)tMax + (float)tMin) / 2.0f - base;
                if (daily > 0.0f) totalGDD += daily;
                Flash_Save(totalGDD, g_cropIndex);
                tMax = 0; tMin = 255; hasData = 0;
            }
            SDLog_NewDay();
            lastDay = now.day;
        }

        /* ---- 刷新显示 ---- */
        UI_Refresh(totalGDD, &now, airTemp, humidity, soilTemp, soilHum, hasRain);

        // 微调循环速度
        for (volatile uint32_t i = 0; i < 5000; i++);
    }
}