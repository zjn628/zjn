#include "soil.h"

/*
 * Soil_Init
 * 功能：初始化PA1为模拟输入，配置ADC1通道1
 */
void Soil_Init(void)
{
    /* 1. 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,  ENABLE);

    /* 2. PA1配置为模拟输入 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin  = SOIL_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(SOIL_GPIO_PORT, &GPIO_InitStructure);

    /* 3. ADC时钟分频：72MHz / 6 = 12MHz */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 4. ADC1基本配置 */
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None; /* 软件触发 */
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 5. 使能ADC1 */
    ADC_Cmd(ADC1, ENABLE);

    /* 6. 上电校准（必须做） */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/*
 * Soil_ReadADC
 * 功能：软件触发单次转换，返回12位ADC原始值（0~4095）
 */
uint16_t Soil_ReadADC(void)
{
    ADC_RegularChannelConfig(ADC1, SOIL_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

/*
 * Soil_ReadADC_Avg
 * 功能：10次采样取平均，降低噪声
 */
uint16_t Soil_ReadADC_Avg(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < SOIL_SAMPLE_COUNT; i++)
        sum += Soil_ReadADC();
    return (uint16_t)(sum / SOIL_SAMPLE_COUNT);
}

/*
 * Soil_GetHumidity
 * 功能：ADC值转湿度百分比
 * 公式：humidity = (ADC_DRY - adc) * 100 / (ADC_DRY - ADC_WET)
 * 返回：0~100（已限幅）
 */
uint8_t Soil_GetHumidity(void)
{
    uint16_t adc = Soil_ReadADC_Avg();
    
    // 注意：电容式土壤传感器通常是 ADC值越大越干燥
    int32_t hum = ((int32_t)(SOIL_ADC_DRY - adc) * 100) 
                  / (SOIL_ADC_DRY - SOIL_ADC_WET);
    
    // 软件限幅防止溢出显示
    if (hum < 0)   hum = 0;
    if (hum > 100) hum = 100;
    
    return (uint8_t)hum;
}