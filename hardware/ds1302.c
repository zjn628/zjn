#include "ds1302.h"

// 引脚定义
#define SCLK_W(x) GPIO_WriteBit(GPIOB, GPIO_Pin_10, (BitAction)x)
#define IO_W(x)   GPIO_WriteBit(GPIOB, GPIO_Pin_11, (BitAction)x)
#define IO_R()    GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11)
#define RST_W(x)  GPIO_WriteBit(GPIOB, GPIO_Pin_12, (BitAction)x)

// 内部底层函数：写一个字节
void DS1302_WriteByte(uint8_t data) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure); // 设置为输出

    for (uint8_t i = 0; i < 8; i++) {
        IO_W(data & 0x01);
        SCLK_W(1); SCLK_W(0);
        data >>= 1;
    }
}

// 内部底层函数：读一个字节
uint8_t DS1302_ReadByte(void) {
    uint8_t data = 0;
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure); // 设置为输入

    for (uint8_t i = 0; i < 8; i++) {
        data >>= 1;
        if (IO_R()) data |= 0x80;
        SCLK_W(1); SCLK_W(0);
    }
    return data;
}

// 辅助：进制转换
uint8_t DecToBCD(uint8_t dec) { return ((dec/10)<<4) | (dec%10); }
uint8_t BCDToDec(uint8_t bcd) { return (bcd>>4)*10 + (bcd&0x0F); }

void DS1302_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    RST_W(0); SCLK_W(0);
}

// 【解决报错的关键函数】
void DS1302_SetTime(DS1302_Time *time) {
    RST_W(1);
    DS1302_WriteByte(0x8E); // 写保护寄存器
    DS1302_WriteByte(0x00); // 允许写入
    RST_W(0);

    uint8_t write_addr = 0x80;
    uint8_t t_data[7];
    t_data[0] = DecToBCD(time->sec);
    t_data[1] = DecToBCD(time->min);
    t_data[2] = DecToBCD(time->hour);
    t_data[3] = DecToBCD(time->day);
    t_data[4] = DecToBCD(time->month);
    t_data[5] = DecToBCD(time->week);
    t_data[6] = DecToBCD(time->year);

    for(int i=0; i<7; i++) {
        RST_W(1);
        DS1302_WriteByte(write_addr);
        DS1302_WriteByte(t_data[i]);
        RST_W(0);
        write_addr += 2;
    }
}

void DS1302_ReadTime(DS1302_Time *time) {
    uint8_t read_addr = 0x81;
    uint8_t res[7];
    for(int i=0; i<7; i++) {
        RST_W(1);
        DS1302_WriteByte(read_addr);
        res[i] = DS1302_ReadByte();
        RST_W(0);
        read_addr += 2;
    }
    time->sec = BCDToDec(res[0] & 0x7F);
    time->min = BCDToDec(res[1]);
    time->hour = BCDToDec(res[2]);
    time->day = BCDToDec(res[3]);
    time->month = BCDToDec(res[4]);
    time->year = BCDToDec(res[6]);
}