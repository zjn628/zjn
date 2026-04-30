#include "dht11.h"

#define DHT11_PORT GPIOB
#define DHT11_PIN  GPIO_Pin_1

static void Delay_us(uint16_t us);
static void DHT11_Set_Output(void);
static void DHT11_Set_Input(void);

void DHT11_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = DHT11_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);

    GPIO_SetBits(DHT11_PORT, DHT11_PIN);
}

static void Delay_us(uint16_t us)
{
    TIM_SetCounter(TIM1, 0);
    while (TIM_GetCounter(TIM1) < us);
}

static void DHT11_Set_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = DHT11_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);
}

static void DHT11_Set_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin  = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DHT11_PORT, &GPIO_InitStructure);
}

uint8_t DHT11_Read_Data(DHT11_DataTypedef *data)
{
    uint8_t i, j;
    uint8_t buf[5] = {0};

    DHT11_Set_Output();
    GPIO_ResetBits(DHT11_PORT, DHT11_PIN);
    Delay_us(20000);
    GPIO_SetBits(DHT11_PORT, DHT11_PIN);
    Delay_us(30);
    DHT11_Set_Input();

    if (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 0)
    {
        while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 0);
        while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 1);

        for (j = 0; j < 5; j++)
        {
            for (i = 0; i < 8; i++)
            {
                while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 0);
                Delay_us(40);

                if (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 1)
                    buf[j] |= (1 << (7 - i));

                while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 1);
            }
        }

        if (buf[0] + buf[1] + buf[2] + buf[3] == buf[4])
        {
            data->humidity    = buf[0];
            data->temperature = buf[2];
            return 1;
        }
    }

    return 0;
}