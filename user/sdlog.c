#include "sdlog.h"
#include "ff.h"
#include "ds1302.h"
#include <stdio.h>

static FIL g_file;
static FATFS g_fs;
static char g_filename[16];

static void SDLog_CreateFileName(DS1302_Time *time)
{
    sprintf(g_filename, "%02d%02d%02d.csv", time->year, time->month, time->day);
}

void SDLog_Init(void)
{
    FRESULT res;
    DS1302_Time time;
    
    DS1302_ReadTime(&time);
    SDLog_CreateFileName(&time);
    
    res = f_mount(&g_fs, "", 1);
    if (res != FR_OK) return;
    
    res = f_open(&g_file, g_filename, FA_OPEN_ALWAYS | FA_WRITE);
    if (res != FR_OK) return;
    
    if (f_size(&g_file) == 0)
    {
        f_printf(&g_file, "时间,空气温度,土壤温度,土壤湿度,总积温,水泵状态,雨水\n");
    }
    
    f_lseek(&g_file, f_size(&g_file));
}

void SDLog_Write(uint8_t airTemp, float soilTemp, uint8_t soilHum, float totalGDD, uint8_t hasRain, uint8_t pumpState, DS1302_Time *time)
{
    if (f_error(&g_file)) return;
    
    f_printf(&g_file, "%02d:%02d:%02d,%d,%.1f,%d,%.1f,%s,%s\n",
             time->hour, time->min, time->sec,
             airTemp,
             soilTemp,
             soilHum,
             totalGDD,
             pumpState ? "ON" : "OFF",
             hasRain ? "Y" : "N");
    
    f_sync(&g_file);
}

void SDLog_NewDay(void)
{
    f_close(&g_file);
    
    DS1302_Time time;
    DS1302_ReadTime(&time);
    SDLog_CreateFileName(&time);
    
    FRESULT res = f_open(&g_file, g_filename, FA_OPEN_ALWAYS | FA_WRITE);
    if (res != FR_OK) return;
    
    if (f_size(&g_file) == 0)
    {
        f_printf(&g_file, "时间,空气温度,土壤温度,土壤湿度,总积温,水泵状态,雨水\n");
    }
    
    f_lseek(&g_file, f_size(&g_file));
}