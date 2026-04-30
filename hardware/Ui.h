#ifndef __UI_H
#define __UI_H

#include "stm32f10x.h"
#include "ds1302.h"

/* ===== 显示页面索引 ===== */
#define PAGE_MONITOR    0
#define PAGE_CROP       1
#define PAGE_SET        2
#define PAGE_NUM        3

/* ===== 设置子项索引 ===== */
#define SET_ITEM_CROP       0   // 屏A: 选择作物
#define SET_ITEM_BASETEMP   1   // 屏A: GDD基准温度
#define SET_ITEM_TEMPMAX    2   // 屏A: GDD上限温度
#define SET_ITEM_STG0       3   // 屏B: 阶段1阈值
#define SET_ITEM_STG1       4   // 屏B: 阶段2阈值
#define SET_ITEM_STG2       5   // 屏B: 阶段3阈值
#define SET_ITEM_STG3       6   // 屏B: 阶段4阈值
#define SET_ITEM_PUMP_ON    7   // 屏C: 水泵温度开启阈值
#define SET_ITEM_PUMP_OFF   8   // 屏C: 水泵温度关闭阈值
#define SET_ITEM_HUM_ON     9   // 屏F: 湿度开泵阈值
#define SET_ITEM_HUM_OFF    10  // 屏F: 湿度关泵阈值
#define SET_ITEM_RESET      11  // 屏D: 清零积温
#define SET_ITEM_TIME_YEAR  12  // 屏E: 年
#define SET_ITEM_TIME_MON   13  // 屏E: 月
#define SET_ITEM_TIME_DAY   14  // 屏E: 日
#define SET_ITEM_TIME_HOUR  15  // 屏E: 时
#define SET_ITEM_TIME_MIN   16  // 屏E: 分
#define SET_ITEM_TIME_SEC   17  // 屏E: 秒
#define SET_ITEM_NUM        18

/* ===== 全局UI状态 ===== */
extern uint8_t  g_page;
extern uint8_t  g_inSet;
extern uint8_t  g_setItem;
extern uint8_t  g_baseTemp;
extern uint8_t  g_tempMax;
extern uint8_t  g_resetConfirm;

/* ===== 对外接口 ===== */
void    UI_Init(void);
void    UI_Refresh(float totalGDD, DS1302_Time *t,
                   uint8_t airTemp, uint8_t humidity,
                   float soilTemp, uint8_t soilHum,
                   uint8_t hasRain);

/* 返回值：0=无操作, 1=清零积温并写Flash, 2=写Flash */
uint8_t UI_HandleKey(uint8_t key, DS1302_Time *t, float *totalGDD);

#endif