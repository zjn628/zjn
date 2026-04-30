/*-----------------------------------------------------------------------
 * spi_sd.c - SPI底层驱动 + SD卡完整驱动实现
 * 支持：SD V1 / SD V2 / SDHC  SPI模式
 *-----------------------------------------------------------------------*/
#include "Spi sd.h"
#include <string.h>

uint8_t SD_Type = SD_TYPE_ERR;  /* SD卡类型全局变量 */

/*=======================================================================
 * SPI底层实现
 *=======================================================================*/

/*-----------------------------------------------------------------------
 * SPI_SD_Init：初始化SPI1及相关GPIO
 * 初始低速（约281kHz），SD卡初始化完成后调用SPI_SetSpeed(1)提速
 *-----------------------------------------------------------------------*/
void SPI_SD_Init(void)
{
    /* 1. 使能时钟 */
    RCC_APB2PeriphClockCmd(SD_GPIO_RCC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB2PeriphClockCmd(SD_SPI_RCC, ENABLE);

    GPIO_InitTypeDef  GPIO_InitStruct;
    SPI_InitTypeDef   SPI_InitStruct;

    /* 2. SCK、MOSI配置为复用推挽输出 */
    GPIO_InitStruct.GPIO_Pin   = SD_SCK_PIN | SD_MOSI_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SD_GPIO, &GPIO_InitStruct);

    /* 3. MISO配置为浮空输入 */
    GPIO_InitStruct.GPIO_Pin  = SD_MISO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SD_GPIO, &GPIO_InitStruct);

    /* 4. CS配置为推挽输出，默认拉高（不选中） */
    GPIO_InitStruct.GPIO_Pin  = SD_CS_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(SD_GPIO, &GPIO_InitStruct);
    SD_CS_HIGH();

    /* 5. SPI1配置
     * 模式：全双工主机
     * CPOL=1, CPHA=1（模式3，SD卡兼容）
     * 初始低速：72MHz / 256 ≈ 281kHz（SD卡初始化要求<400kHz）
     * 数据宽度：8bit，MSB先行 */
    SPI_InitStruct.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStruct.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStruct.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStruct.SPI_CPOL              = SPI_CPOL_High;
    SPI_InitStruct.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStruct.SPI_NSS               = SPI_NSS_Soft;      /* 软件控制CS */
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; /* 低速 */
    SPI_InitStruct.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStruct.SPI_CRCPolynomial     = 7;
    SPI_Init(SD_SPI, &SPI_InitStruct);
    SPI_Cmd(SD_SPI, ENABLE);
}

/*-----------------------------------------------------------------------
 * SPI_SetSpeed：切换SPI时钟速率
 * fast=1：高速（72MHz/4 = 18MHz）
 * fast=0：低速（72MHz/256 ≈ 281kHz）
 *-----------------------------------------------------------------------*/
void SPI_SetSpeed(uint8_t fast)
{
    /* 修改SPI_CR1寄存器的波特率位（bit5:3） */
    SPI_Cmd(SD_SPI, DISABLE);
    if (fast)
        SD_SPI->CR1 = (SD_SPI->CR1 & ~SPI_BaudRatePrescaler_256)
                      | SPI_BaudRatePrescaler_4;    /* 18MHz */
    else
        SD_SPI->CR1 = (SD_SPI->CR1 & ~SPI_BaudRatePrescaler_256)
                      | SPI_BaudRatePrescaler_256;  /* 281kHz */
    SPI_Cmd(SD_SPI, ENABLE);
}

/*-----------------------------------------------------------------------
 * SPI_ReadWriteByte：SPI收发一个字节（全双工）
 * dat：发送的数据；返回：接收到的数据
 * 发0xFF = 纯读操作（SD卡协议规定）
 *-----------------------------------------------------------------------*/
uint8_t SPI_ReadWriteByte(uint8_t dat)
{
    /* 等待发送缓冲区空 */
    while (SPI_I2S_GetFlagStatus(SD_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SD_SPI, dat);
    /* 等待接收缓冲区非空 */
    while (SPI_I2S_GetFlagStatus(SD_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SD_SPI);
}

/*=======================================================================
 * SD卡内部辅助函数
 *=======================================================================*/

/*-----------------------------------------------------------------------
 * SD_WaitNotBusy：等待SD卡不忙（读到0xFF表示就绪）
 * 返回：0=就绪，1=超时
 *-----------------------------------------------------------------------*/
static uint8_t SD_WaitNotBusy(void)
{
    uint32_t timeout = 500000;
    while (SPI_ReadWriteByte(0xFF) != 0xFF)
    {
        if (--timeout == 0) return 1;
    }
    return 0;
}

/*-----------------------------------------------------------------------
 * SD_SendCmd：发送6字节命令帧并获取R1响应
 * cmd：命令（含0x40前缀）  arg：32位参数  crc：CRC字节
 * 返回：R1响应字节（0x00=正常，bit7=1表示无响应）
 *-----------------------------------------------------------------------*/
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t  r1;
    uint8_t  retry = 0;

    /* 等待SD卡就绪（写操作后需要等待） */
    SD_WaitNotBusy();

    /* 发送命令帧（6字节）：命令 + 4字节参数 + CRC */
    SPI_ReadWriteByte(cmd);
    SPI_ReadWriteByte((uint8_t)(arg >> 24));
    SPI_ReadWriteByte((uint8_t)(arg >> 16));
    SPI_ReadWriteByte((uint8_t)(arg >>  8));
    SPI_ReadWriteByte((uint8_t)(arg >>  0));
    SPI_ReadWriteByte(crc);

    /* 等待R1响应（最多8个字节，bit7=0表示有效响应） */
    do {
        r1 = SPI_ReadWriteByte(0xFF);
    } while ((r1 & 0x80) && ++retry < 200);

    return r1;
}

/*-----------------------------------------------------------------------
 * SD_GetResponse：等待指定数据令牌
 * token：期望的令牌（如0xFE）
 * 返回：0=收到期望令牌，1=超时
 *-----------------------------------------------------------------------*/
static uint8_t SD_GetResponse(uint8_t token)
{
    uint32_t timeout = 200000;
    uint8_t  res;
    do {
        res = SPI_ReadWriteByte(0xFF);
    } while (res != token && --timeout);
    return (res == token) ? 0 : 1;
}

/*=======================================================================
 * SD卡对外接口实现
 *=======================================================================*/

/*-----------------------------------------------------------------------
 * SD_Init：完整的SD卡SPI模式初始化流程
 * 返回：0=成功，非0=失败
 *
 * 初始化流程：
 * 1. 上电延时（发≥74个CLK，CS高）
 * 2. CMD0复位，进入SPI模式（期望返回0x01）
 * 3. CMD8检测SD V2（响应0x01=SD V2，0x05=SD V1/MMC）
 * 4. ACMD41初始化（SD卡），等待返回0x00
 * 5. CMD58读OCR，判断SDHC
 * 6. 非SDHC卡设置块长度512（CMD16）
 * 7. SPI提速
 *-----------------------------------------------------------------------*/
uint8_t SD_Init(void)
{
    uint8_t  r1;
    uint8_t  buf[4];
    uint16_t retry;

    SD_Type = SD_TYPE_ERR;

    /* 1. 上电延时：CS拉高，发送至少74个CLK让SD卡完成上电复位 */
    SD_CS_HIGH();
    for (uint8_t i = 0; i < 10; i++)
        SPI_ReadWriteByte(0xFF);    /* 10 * 8bit = 80个CLK */

    /* 2. CMD0：复位SD卡进入SPI模式
     * CS拉低，期望R1=0x01（空闲状态）
     * CRC必须正确：0x95是CMD0的有效CRC */
    retry = 0;
    do {
        SD_CS_LOW();
        r1 = SD_SendCmd(CMD0, 0, 0x95);
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);    /* 额外时钟，释放总线 */
    } while (r1 != 0x01 && ++retry < 200);

    if (r1 != 0x01)
        return 1;   /* CMD0失败：SD卡未响应，检查接线 */

    /* 3. CMD8：发送接口条件，检测是否为SD V2
     * 参数0x000001AA：电压范围2.7~3.6V，检查模式0xAA
     * CRC 0x87是CMD8的有效CRC */
    SD_CS_LOW();
    r1 = SD_SendCmd(CMD8, 0x000001AA, 0x87);

    if (r1 == 0x01)
    {
        /* SD V2：读R7响应（4字节），验证电压和回显 */
        for (uint8_t i = 0; i < 4; i++)
            buf[i] = SPI_ReadWriteByte(0xFF);
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);

        if (buf[2] == 0x01 && buf[3] == 0xAA)
        {
            /* 电压和回显正确，为SD V2，发送ACMD41初始化 */
            retry = 0;
            do {
                SD_CS_LOW();
                SD_SendCmd(CMD55, 0, 0x01);     /* ACMD前缀 */
                SD_CS_HIGH();
                SPI_ReadWriteByte(0xFF);

                SD_CS_LOW();
                /* HCS=1（bit30）：告知SD卡主机支持SDHC */
                r1 = SD_SendCmd(CMD41, 0x40000000, 0x01);
                SD_CS_HIGH();
                SPI_ReadWriteByte(0xFF);
            } while (r1 != 0x00 && ++retry < 500);

            if (r1 != 0x00)
                return 2;   /* ACMD41超时：卡损坏或不支持 */

            /* CMD58：读OCR寄存器，判断是否为SDHC */
            SD_CS_LOW();
            r1 = SD_SendCmd(CMD58, 0, 0x01);
            for (uint8_t i = 0; i < 4; i++)
                buf[i] = SPI_ReadWriteByte(0xFF);
            SD_CS_HIGH();
            SPI_ReadWriteByte(0xFF);

            /* OCR[30]（CSS位）=1：SDHC/SDXC；=0：SD V2普通卡 */
            if (r1 == 0x00)
                SD_Type = (buf[0] & 0x40) ? SD_TYPE_HC : SD_TYPE_V2;
            else
                return 3;
        }
        else
        {
            SD_CS_HIGH();
            SPI_ReadWriteByte(0xFF);
            return 4;   /* CMD8电压不匹配 */
        }
    }
    else
    {
        /* CMD8返回非0x01：SD V1 或 MMC */
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);

        retry = 0;
        do {
            SD_CS_LOW();
            SD_SendCmd(CMD55, 0, 0x01);
            SD_CS_HIGH();
            SPI_ReadWriteByte(0xFF);

            SD_CS_LOW();
            r1 = SD_SendCmd(CMD41, 0, 0x01);
            SD_CS_HIGH();
            SPI_ReadWriteByte(0xFF);
        } while (r1 != 0x00 && ++retry < 500);

        SD_Type = (r1 == 0x00) ? SD_TYPE_V1 : SD_TYPE_ERR;
        if (SD_Type == SD_TYPE_ERR)
            return 5;
    }

    /* 4. 非SDHC卡：块长度设置为512字节 */
    if (SD_Type != SD_TYPE_HC)
    {
        SD_CS_LOW();
        r1 = SD_SendCmd(CMD16, 512, 0x01);
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
        if (r1 != 0x00) return 6;
    }

    /* 5. 初始化成功，SPI切换到高速模式 */
    SPI_SetSpeed(1);
    return 0;
}

/*-----------------------------------------------------------------------
 * SD_ReadBlock：读取一个512字节扇区
 * buf：接收缓冲区（必须512字节）
 * sector：扇区号（SDHC用块地址，其他卡自动转为字节地址）
 * 返回：0=成功，非0=失败
 *-----------------------------------------------------------------------*/
uint8_t SD_ReadBlock(uint8_t *buf, uint32_t sector)
{
    uint8_t r1;

    /* 非SDHC卡：扇区号转字节地址（sector * 512） */
    if (SD_Type != SD_TYPE_HC)
        sector <<= 9;

    SD_CS_LOW();

    /* 发送CMD17（读单块） */
    r1 = SD_SendCmd(CMD17, sector, 0x01);
    if (r1 != 0x00)
    {
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
        return 1;
    }

    /* 等待数据令牌0xFE（超时约100ms） */
    if (SD_GetResponse(0xFE) != 0)
    {
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
        return 2;
    }

    /* 读取512字节数据 */
    for (uint16_t i = 0; i < 512; i++)
        buf[i] = SPI_ReadWriteByte(0xFF);

    /* 读取2字节CRC（丢弃，SPI模式不校验） */
    SPI_ReadWriteByte(0xFF);
    SPI_ReadWriteByte(0xFF);

    SD_CS_HIGH();
    SPI_ReadWriteByte(0xFF);    /* 释放总线 */
    return 0;
}

/*-----------------------------------------------------------------------
 * SD_WriteBlock：写入一个512字节扇区
 * buf：写入数据（512字节）
 * sector：扇区号
 * 返回：0=成功，非0=失败
 *-----------------------------------------------------------------------*/
uint8_t SD_WriteBlock(const uint8_t *buf, uint32_t sector)
{
    uint8_t  r1;
    uint8_t  dataResp;

    if (SD_Type != SD_TYPE_HC)
        sector <<= 9;

    SD_CS_LOW();

    /* 发送CMD24（写单块） */
    r1 = SD_SendCmd(CMD24, sector, 0x01);
    if (r1 != 0x00)
    {
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
        return 1;
    }

    /* 发送1字节空时钟（SD卡规范要求命令后至少1个字节间隔） */
    SPI_ReadWriteByte(0xFF);

    /* 发送数据令牌0xFE，表示数据块开始 */
    SPI_ReadWriteByte(0xFE);

    /* 发送512字节数据 */
    for (uint16_t i = 0; i < 512; i++)
        SPI_ReadWriteByte(buf[i]);

    /* 发送伪CRC（SPI模式可以随意，SD卡不检查） */
    SPI_ReadWriteByte(0xFF);
    SPI_ReadWriteByte(0xFF);

    /* 读取数据响应字节（格式：xxx00101b=接受，xxx01011b=CRC错误，xxx01101b=写错误） */
    dataResp = SPI_ReadWriteByte(0xFF) & 0x1F;
    if (dataResp != 0x05)   /* 0x05 = 0b00000101 = 数据被接受 */
    {
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
        return 2;
    }

    /* 等待写入完成（SD卡内部Flash写入，期间MISO为低电平） */
    if (SD_WaitNotBusy() != 0)
    {
        SD_CS_HIGH();
        SPI_ReadWriteByte(0xFF);
        return 3;   /* 写入超时 */
    }

    SD_CS_HIGH();
    SPI_ReadWriteByte(0xFF);
    return 0;
}

/*-----------------------------------------------------------------------
 * SD_GetSectorCount：通过读CSD寄存器获取扇区总数
 * count：输出扇区数量
 * 返回：0=成功
 *-----------------------------------------------------------------------*/
uint8_t SD_GetSectorCount(uint32_t *count)
{
    uint8_t csd[16];
    uint8_t r1;

    SD_CS_LOW();
    r1 = SD_SendCmd(CMD9, 0, 0x01);
    if (r1 != 0x00) { SD_CS_HIGH(); return 1; }

    if (SD_GetResponse(0xFE) != 0) { SD_CS_HIGH(); return 2; }

    for (uint8_t i = 0; i < 16; i++)
        csd[i] = SPI_ReadWriteByte(0xFF);
    SPI_ReadWriteByte(0xFF);
    SPI_ReadWriteByte(0xFF);

    SD_CS_HIGH();
    SPI_ReadWriteByte(0xFF);

    /* CSD V2（SDHC）：C_SIZE在csd[7:9] */
    if ((csd[0] & 0xC0) == 0x40)
    {
        uint32_t csize = ((uint32_t)(csd[7] & 0x3F) << 16) |
                         ((uint32_t)csd[8] << 8) | csd[9];
        *count = (csize + 1) << 10;     /* 单位：512字节扇区 */
    }
    else    /* CSD V1 */
    {
        uint8_t  n     = (csd[5] & 0x0F) + ((csd[10] & 0x80) >> 7) +
                         ((csd[9]  & 0x03) << 1) + 2;
        uint32_t csize = ((uint32_t)(csd[6] & 0x03) << 10) |
                         ((uint32_t)csd[7] << 2) |
                         ((csd[8] & 0xC0) >> 6);
        *count = (csize + 1) << (n - 9);
    }

    return 0;
}