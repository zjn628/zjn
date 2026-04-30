#include "ui.h"
#include "OLED.h"
#include "crop.h"
#include "ds1302.h"
#include "key.h"
#include "relay.h"

/* ===== 全局UI状态 ===== */
uint8_t  g_page         = PAGE_MONITOR;
uint8_t  g_inSet        = 0;
uint8_t  g_setItem      = SET_ITEM_CROP;
uint8_t  g_baseTemp     = 10;
uint8_t  g_tempMax      = 30;
uint8_t  g_resetConfirm = 0;

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

static void ShowTwoDigit(uint8_t line, uint8_t col, uint8_t val)
{
    OLED_ShowNum(line, col,     val / 10, 1);
    OLED_ShowNum(line, col + 1, val % 10, 1);
}

static void ShowProgressBar(uint8_t line, uint8_t col, uint8_t progress)
{
    uint8_t filled = (uint8_t)((uint16_t)progress * 6 / 100);
    OLED_ShowChar(line, col, '[');
    for (uint8_t i = 0; i < 6; i++)
        OLED_ShowChar(line, col + 1 + i, (i < filled) ? '#' : ' ');
    OLED_ShowChar(line, col + 7, ']');
}

static uint8_t IncVal8(uint8_t v, uint8_t mn, uint8_t mx)
{ return (v >= mx) ? mn : v + 1; }

static uint8_t DecVal8(uint8_t v, uint8_t mn, uint8_t mx)
{ return (v <= mn) ? mx : v - 1; }

static uint16_t IncVal16(uint16_t v, uint16_t mn, uint16_t mx, uint16_t step)
{ return (v + step > mx) ? mx : v + step; }

static uint16_t DecVal16(uint16_t v, uint16_t mn, uint16_t mx, uint16_t step)
{ return (v < mn + step) ? mn : v - step; }

/* ================================================================
 * UI_Init
 * ================================================================ */
void UI_Init(void)
{
    OLED_Clear();
    g_baseTemp = Crop_GetBaseTemp();
    g_tempMax  = 30;
}

/* ================================================================
 * 普通显示页面
 * ================================================================ */

/*
 * Page 0 — 主监测页
 * L1: D:xx S:xx.x H:xx%
 * L2: 20xx/xx/xx
 * L3: GDD:xxxxx Rn:Y/N
 * L4: xx:xx:xx Pmp:ON
 */
static void Page_Monitor(float totalGDD, DS1302_Time *t,
                         uint8_t airTemp, uint8_t humidity,
                         float soilTemp, uint8_t soilHum,
                         uint8_t hasRain)
{
    /* L1 气温 + 土温 + 土壤湿度 */
    OLED_ShowString(1, 1, "D:");
    ShowTwoDigit(1, 3, airTemp);
    OLED_ShowString(1, 5, " S:");
    OLED_ShowNum(1, 8,  (uint32_t)soilTemp, 2);
    OLED_ShowChar(1, 10, '.');
    OLED_ShowNum(1, 11, (uint32_t)(soilTemp * 10) % 10, 1);

    /* L2 日期 */
    OLED_ShowString(2, 1, "20");
    ShowTwoDigit(2, 3, t->year);
    OLED_ShowChar(2, 5, '/');
    ShowTwoDigit(2, 6, t->month);
    OLED_ShowChar(2, 8, '/');
    ShowTwoDigit(2, 9, t->day);
	OLED_ShowString(2, 11, " Rn:");
    OLED_ShowChar(2, 15, hasRain ? 'Y' : 'N');
    OLED_ShowChar(2, 16, ' ');
	
	/* L3 积温（左）+ 土壤湿度（右） */
    OLED_ShowString(3, 1, "GDD:");
    OLED_ShowNum(3, 5, (uint32_t)totalGDD, 4);   // 4位，留出右侧空间
    OLED_ShowString(3, 9, " H:");
    OLED_ShowNum(3, 12, soilHum, 3);
    OLED_ShowChar(3, 15, '%');

    /* L4 时间 + 水泵状态 */
    ShowTwoDigit(4, 1, t->hour);
    OLED_ShowChar(4, 3, ':');
    ShowTwoDigit(4, 4, t->min);
    OLED_ShowChar(4, 6, ':');
    ShowTwoDigit(4, 7, t->sec);
    OLED_ShowString(4, 9, (g_pumpState == PUMP_ON) ? "Pmp:ON " : "Pmp:OFF");
}

/*
 * Page 1 — 作物阶段页
 */
static void Page_Crop(float totalGDD)
{
    const CropPreset *p   = &g_cropPresets[g_cropIndex];
    uint8_t           st  = Crop_GetStage(totalGDD);
    uint8_t           pct = Crop_GetProgress(totalGDD);
 
    OLED_ShowString(1, 1, "Crop:");
    OLED_ShowString(1, 6, "          ");
    OLED_ShowString(1, 6, (char*)p->name);
 
    OLED_ShowString(2, 1, "Stg:");
    OLED_ShowString(2, 5, "            ");
    OLED_ShowString(2, 5, (char*)p->stageName[st]);
 
    OLED_ShowString(3, 1, "GDD:");
    OLED_ShowNum(3, 5, (uint32_t)totalGDD, 4);
    OLED_ShowChar(3, 9, '/');
    OLED_ShowNum(3, 10, p->stageGDD[st], 4);
 
    ShowProgressBar(4, 1, pct);
    OLED_ShowNum(4, 10, pct, 3);
    OLED_ShowChar(4, 13, '%');
}
 
/* Page 2 — 设置提示页 */
static void Page_SetHint(void)
{
    OLED_ShowString(1, 1, "Long press OK   ");
    OLED_ShowString(2, 1, "to enter setup  ");
    OLED_ShowString(3, 1, "                ");
    OLED_ShowString(4, 1, "                ");
}
 
/* ================================================================
 * 设置页面
 * ================================================================ */
 
/*
 * 屏A — 作物与GDD参数（左对齐竖排）
 * L1:   Crop & GDD
 * L2: > Crop: Wheat
 * L3:   Base: 10 C
 * L4:   Max:  30 C
 */
static void Page_SetA(void)
{
    OLED_ShowString(1, 1, "  Crop & GDD    ");
 
    OLED_ShowChar(2, 1, (g_setItem == SET_ITEM_CROP)     ? '>' : ' ');
    OLED_ShowString(2, 2, " Crop: ");
    OLED_ShowString(2, 9, "        ");
    OLED_ShowString(2, 9, (char*)g_cropPresets[g_cropIndex].name);
 
    OLED_ShowChar(3, 1, (g_setItem == SET_ITEM_BASETEMP) ? '>' : ' ');
    OLED_ShowString(3, 2, " Base: ");
    OLED_ShowNum(3, 9, g_baseTemp, 2);
    OLED_ShowString(3, 12, " C  ");
 
    OLED_ShowChar(4, 1, (g_setItem == SET_ITEM_TEMPMAX)  ? '>' : ' ');
    OLED_ShowString(4, 2, "  Max: ");
    OLED_ShowNum(4, 9, g_tempMax, 2);
    OLED_ShowString(4, 12, " C  ");
}
 
/*
 * 屏B — 阶段阈值
 * L1: Stg thresholds
 * L2: >S1:xxxx S2:xxxx
 * L3: >S3:xxxx S4:xxxx
 * L4: UP/DN step=10
 */
static void Page_SetB(void)
{
    CropPreset *cp = (CropPreset*)&g_cropPresets[g_cropIndex];
 
    OLED_ShowString(1, 1, "Stg thresholds  ");
 
    OLED_ShowChar(2, 1, (g_setItem == SET_ITEM_STG0) ? '>' : ' ');
    OLED_ShowString(2, 2, "S1:");
    OLED_ShowNum(2, 5, cp->stageGDD[0], 4);
    OLED_ShowChar(2, 9, (g_setItem == SET_ITEM_STG1) ? '>' : ' ');
    OLED_ShowString(2, 10, "S2:");
    OLED_ShowNum(2, 13, cp->stageGDD[1], 4);
 
    OLED_ShowChar(3, 1, (g_setItem == SET_ITEM_STG2) ? '>' : ' ');
    OLED_ShowString(3, 2, "S3:");
    OLED_ShowNum(3, 5, cp->stageGDD[2], 4);
    OLED_ShowChar(3, 9, (g_setItem == SET_ITEM_STG3) ? '>' : ' ');
    OLED_ShowString(3, 10, "S4:");
    OLED_ShowNum(3, 13, cp->stageGDD[3], 4);
 
    OLED_ShowString(4, 1, "UP/DN step=10   ");
}
 
/*
 * 屏C — 水泵温度阈值
 * L1:  Pump Temp
 * L2: > ON: 30 deg
 * L3:  OFF: 28 deg
 * L4: ON>OFF,step=1
 */
static void Page_SetC(void)
{
    OLED_ShowString(1, 1, "Soil Temp Pump  ");
 
    OLED_ShowChar(2, 1, (g_setItem == SET_ITEM_PUMP_ON)  ? '>' : ' ');
    OLED_ShowString(2, 2, "ON :");
    ShowTwoDigit(2, 6, g_pumpTempOn);
    OLED_ShowString(2, 8, " deg    ");
 
    OLED_ShowChar(3, 1, (g_setItem == SET_ITEM_PUMP_OFF) ? '>' : ' ');
    OLED_ShowString(3, 2, "OFF:");
    ShowTwoDigit(3, 6, g_pumpTempOff);
    OLED_ShowString(3, 8, " deg    ");
 
    OLED_ShowString(4, 1, "step=1          ");
}
 
/*
 * 屏F — 湿度开泵/关泵阈值（新增）
 * L1:  Hum Threshold
 * L2: > ON: xx% (low)
 * L3:  OFF: xx% (high)
 * L4: ON<OFF,step=1
 *
 * 注意：湿度逻辑和温度相反
 *   湿度 < ON阈值 → 开泵（土太干了）
 *   湿度 > OFF阈值 → 关泵（土够湿了）
 *   所以 ON < OFF
 */
static void Page_SetF(void)
{
    OLED_ShowString(1, 1, " Hum Threshold  ");
 
    OLED_ShowChar(2, 1, (g_setItem == SET_ITEM_HUM_ON)  ? '>' : ' ');
    OLED_ShowString(2, 2, "ON :");
    ShowTwoDigit(2, 6, g_pumpHumOn);
    OLED_ShowString(2, 8, "%       ");
 
    OLED_ShowChar(3, 1, (g_setItem == SET_ITEM_HUM_OFF) ? '>' : ' ');
    OLED_ShowString(3, 2, "OFF:");
    ShowTwoDigit(3, 6, g_pumpHumOff);
    OLED_ShowString(3, 8, "%       ");
 
    OLED_ShowString(4, 1, "ON<OFF,step=1   ");
}
 
/*
 * 屏D — 清零积温（固定5位补零，防闪烁）
 */
static void Page_SetD(float totalGDD)
{
    OLED_ShowString(1, 1, "  Reset GDD?    ");
    OLED_ShowString(2, 1, " GDD: ");
    OLED_ShowNum(2, 7, (uint32_t)totalGDD, 5);
    OLED_ShowString(2, 12, "     ");
    OLED_ShowString(3, 1, "                ");
    if (g_resetConfirm)
        OLED_ShowString(4, 1, "  [YES]    NO   ");
    else
        OLED_ShowString(4, 1, "   YES    [NO]  ");
}
 
/*
 * 屏E — 校时
 */
static void Page_SetE(DS1302_Time *t)
{
    OLED_ShowString(1, 1, "Set Date & Time ");
 
    OLED_ShowChar(2, 1, (g_setItem == SET_ITEM_TIME_YEAR) ? '>' : ' ');
    ShowTwoDigit(2, 2, t->year);
    OLED_ShowChar(2, 4, '/');
    OLED_ShowChar(2, 5, (g_setItem == SET_ITEM_TIME_MON)  ? '>' : ' ');
    ShowTwoDigit(2, 6, t->month);
    OLED_ShowChar(2, 8, '/');
    OLED_ShowChar(2, 9, (g_setItem == SET_ITEM_TIME_DAY)  ? '>' : ' ');
    ShowTwoDigit(2, 10, t->day);
    OLED_ShowString(2, 12, "    ");
 
    OLED_ShowChar(3, 1, (g_setItem == SET_ITEM_TIME_HOUR) ? '>' : ' ');
    ShowTwoDigit(3, 2, t->hour);
    OLED_ShowChar(3, 4, ':');
    OLED_ShowChar(3, 5, (g_setItem == SET_ITEM_TIME_MIN)  ? '>' : ' ');
    ShowTwoDigit(3, 6, t->min);
    OLED_ShowChar(3, 8, ':');
    OLED_ShowChar(3, 9, (g_setItem == SET_ITEM_TIME_SEC)  ? '>' : ' ');
    ShowTwoDigit(3, 10, t->sec);
    OLED_ShowString(3, 12, "    ");
 
    OLED_ShowString(4, 1, "OK=write DS1302 ");
}
 
/* ================================================================
 * UI_Refresh
 * ================================================================ */
void UI_Refresh(float totalGDD, DS1302_Time *t,
                uint8_t airTemp, uint8_t humidity,
                float soilTemp, uint8_t soilHum,
                uint8_t hasRain)
{
    if (g_inSet)
    {
        if      (g_setItem <= SET_ITEM_TEMPMAX)  Page_SetA();
        else if (g_setItem <= SET_ITEM_STG3)     Page_SetB();
        else if (g_setItem <= SET_ITEM_PUMP_OFF) Page_SetC();
        else if (g_setItem <= SET_ITEM_HUM_OFF)  Page_SetF();
        else if (g_setItem == SET_ITEM_RESET)    Page_SetD(totalGDD);
        else                                     Page_SetE(t);
        return;
    }
 
    switch (g_page)
    {
        case PAGE_MONITOR:
            Page_Monitor(totalGDD, t, airTemp, humidity, soilTemp, soilHum, hasRain);
            break;
        case PAGE_CROP:  Page_Crop(totalGDD); break;
        case PAGE_SET:   Page_SetHint();      break;
        default: break;
    }
}
 
/* ================================================================
 * UI_HandleKey
 * ================================================================ */
uint8_t UI_HandleKey(uint8_t key, DS1302_Time *t, float *totalGDD)
{
    /* -------- 普通浏览模式 -------- */
    if (!g_inSet)
    {
        if (key == KEY_MODE)
        {
            g_page = (g_page + 1) % PAGE_NUM;
            OLED_Clear();
        }
        else if (key == KEY_OK_LONG)
        {
            g_inSet   = 1;
            g_setItem = SET_ITEM_CROP;
            OLED_Clear();
        }
        return 0;
    }
 
    /* -------- 长按OK退出设置 -------- */
    if (key == KEY_OK_LONG)
    {
        if (g_setItem >= SET_ITEM_TIME_YEAR)
            DS1302_SetTime(t);
        g_inSet = 0;
        g_page  = PAGE_MONITOR;
        OLED_Clear();
        return 2;
    }
 
    /* -------- MODE键：切换设置项（跨屏清屏） -------- */
    if (key == KEY_MODE)
    {
        if (g_setItem >= SET_ITEM_TIME_YEAR)
            DS1302_SetTime(t);
 
        /* 判断当前所在屏 */
        uint8_t prevScreen = (g_setItem <= SET_ITEM_TEMPMAX)  ? 0 :
                             (g_setItem <= SET_ITEM_STG3)      ? 1 :
                             (g_setItem <= SET_ITEM_PUMP_OFF)  ? 2 :
                             (g_setItem <= SET_ITEM_HUM_OFF)   ? 5 :
                             (g_setItem == SET_ITEM_RESET)     ? 3 : 4;
 
        g_setItem = (g_setItem + 1) % SET_ITEM_NUM;
 
        uint8_t newScreen  = (g_setItem <= SET_ITEM_TEMPMAX)  ? 0 :
                             (g_setItem <= SET_ITEM_STG3)      ? 1 :
                             (g_setItem <= SET_ITEM_PUMP_OFF)  ? 2 :
                             (g_setItem <= SET_ITEM_HUM_OFF)   ? 5 :
                             (g_setItem == SET_ITEM_RESET)     ? 3 : 4;
 
        if (prevScreen != newScreen) OLED_Clear();
        return 0;
    }
 
    /* -------- OK短按 -------- */
    if (key == KEY_OK)
    {
        if (g_setItem == SET_ITEM_RESET && g_resetConfirm)
        {
            *totalGDD      = 0.0f;
            g_resetConfirm = 0;
            return 1;
        }
        if (g_setItem >= SET_ITEM_TIME_YEAR)
            DS1302_SetTime(t);
        return 0;
    }
 
    /* -------- UP / DOWN -------- */
    if (key != KEY_UP && key != KEY_DOWN) return 0;
 
    #define ADJ8(v,mn,mx)       ((key==KEY_UP)?IncVal8(v,mn,mx):DecVal8(v,mn,mx))
    #define ADJ16(v,mn,mx,step) ((key==KEY_UP)?IncVal16(v,mn,mx,step):DecVal16(v,mn,mx,step))
 
    CropPreset *cp = (CropPreset*)&g_cropPresets[g_cropIndex];
 
    switch (g_setItem)
    {
        /* 屏A */
        case SET_ITEM_CROP:
            g_cropIndex = ADJ8(g_cropIndex, 0, CROP_NUM - 1);
            g_baseTemp  = Crop_GetBaseTemp();
            break;
        case SET_ITEM_BASETEMP: g_baseTemp = ADJ8(g_baseTemp, 0,  40); break;
        case SET_ITEM_TEMPMAX:  g_tempMax  = ADJ8(g_tempMax,  20, 45); break;
 
        /* 屏B */
        case SET_ITEM_STG0:
            cp->stageGDD[0] = ADJ16(cp->stageGDD[0], 10, cp->stageGDD[1]-10, 10);
            break;
        case SET_ITEM_STG1:
            cp->stageGDD[1] = ADJ16(cp->stageGDD[1], cp->stageGDD[0]+10, cp->stageGDD[2]-10, 10);
            break;
        case SET_ITEM_STG2:
            cp->stageGDD[2] = ADJ16(cp->stageGDD[2], cp->stageGDD[1]+10, cp->stageGDD[3]-10, 10);
            break;
        case SET_ITEM_STG3:
            cp->stageGDD[3] = ADJ16(cp->stageGDD[3], cp->stageGDD[2]+10, 2000, 10);
            break;
 
        /* 屏C：温度阈值，ON和OFF独立调整，范围0~50 */
        case SET_ITEM_PUMP_ON:
            g_pumpTempOn = ADJ8(g_pumpTempOn, 0, 50);
            break;
        case SET_ITEM_PUMP_OFF:
            g_pumpTempOff = ADJ8(g_pumpTempOff, 0, 50);
            break;
 
        /* 屏F：湿度阈值，ON和OFF独立调整，范围0~100 */
        case SET_ITEM_HUM_ON:
            g_pumpHumOn = ADJ8(g_pumpHumOn, 0, 100);
            break;
        case SET_ITEM_HUM_OFF:
            g_pumpHumOff = ADJ8(g_pumpHumOff, 0, 100);
            break;
 
        /* 屏D */
        case SET_ITEM_RESET:
            g_resetConfirm = ADJ8(g_resetConfirm, 0, 1);
            break;
 
        /* 屏E */
        case SET_ITEM_TIME_YEAR:  t->year  = ADJ8(t->year,  0,  99); break;
        case SET_ITEM_TIME_MON:   t->month = ADJ8(t->month, 1,  12); break;
        case SET_ITEM_TIME_DAY:   t->day   = ADJ8(t->day,   1,  31); break;
        case SET_ITEM_TIME_HOUR:  t->hour  = ADJ8(t->hour,  0,  23); break;
        case SET_ITEM_TIME_MIN:   t->min   = ADJ8(t->min,   0,  59); break;
        case SET_ITEM_TIME_SEC:   t->sec   = ADJ8(t->sec,   0,  59); break;
 
        default: break;
    }
 
    #undef ADJ8
    #undef ADJ16
 
    return 0;
}