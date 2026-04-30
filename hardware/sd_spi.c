#include "sd_spi.h"
#include "Delay.h"

#define SD_SPI_PORT         GPIOA
#define SD_SPI_PIN_CS       GPIO_Pin_4
#define SD_SPI_PIN_SCK      GPIO_Pin_5
#define SD_SPI_PIN_MISO     GPIO_Pin_6
#define SD_SPI_PIN_MOSI     GPIO_Pin_7

#define SD_CS_LOW()         GPIO_ResetBits(SD_SPI_PORT, SD_SPI_PIN_CS)
#define SD_CS_HIGH()        GPIO_SetBits(SD_SPI_PORT, SD_SPI_PIN_CS)

#define SD_DUMMY_BYTE       0xFF

#define SD_CMD0             0
#define SD_CMD1             1
#define SD_CMD8             8
#define SD_CMD9             9
#define SD_CMD12            12
#define SD_CMD16            16
#define SD_CMD17            17
#define SD_CMD24            24
#define SD_CMD55            55
#define SD_CMD58            58
#define SD_ACMD41           0x80 + 41

static uint8_t s_sdReady = 0;
static uint8_t s_sdHighCapacity = 0;
static uint32_t s_sectorCount = 0;

static uint8_t SD_SPI_TxRx(uint8_t txData)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, txData);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

static void SD_SPI_SetSpeedLow(void)
{
    SPI_Cmd(SPI1, DISABLE);
    SPI1->CR1 &= ~SPI_BaudRatePrescaler_256;
    SPI1->CR1 |= SPI_BaudRatePrescaler_256;
    SPI_Cmd(SPI1, ENABLE);
}

static void SD_SPI_SetSpeedHigh(void)
{
    SPI_Cmd(SPI1, DISABLE);
    SPI1->CR1 &= ~SPI_BaudRatePrescaler_256;
    SPI1->CR1 |= SPI_BaudRatePrescaler_4;
    SPI_Cmd(SPI1, ENABLE);
}

static uint8_t SD_WaitReady(uint32_t timeoutMs)
{
    uint8_t res;
    do
    {
        res = SD_SPI_TxRx(SD_DUMMY_BYTE);
        if (res == 0xFF) return 1;
        Delay_ms(1);
    } while (timeoutMs--);

    return 0;
}

static void SD_Deselect(void)
{
    SD_CS_HIGH();
    SD_SPI_TxRx(SD_DUMMY_BYTE);
}

static uint8_t SD_Select(void)
{
    SD_CS_LOW();
    SD_SPI_TxRx(SD_DUMMY_BYTE);
    return SD_WaitReady(200);
}

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
    uint8_t i;
    uint8_t res;
    uint8_t crc = 0x01;

    if (cmd & 0x80)
    {
        cmd &= 0x7F;
        res = SD_SendCmd(SD_CMD55, 0);
        if (res > 1) return res;
    }

    SD_Deselect();
    if (!SD_Select()) return 0xFF;

    if (cmd == SD_CMD0) crc = 0x95;
    if (cmd == SD_CMD8) crc = 0x87;

    SD_SPI_TxRx(0x40 | cmd);
    SD_SPI_TxRx((uint8_t)(arg >> 24));
    SD_SPI_TxRx((uint8_t)(arg >> 16));
    SD_SPI_TxRx((uint8_t)(arg >> 8));
    SD_SPI_TxRx((uint8_t)arg);
    SD_SPI_TxRx(crc);

    if (cmd == SD_CMD12) SD_SPI_TxRx(SD_DUMMY_BYTE);

    i = 10;
    do
    {
        res = SD_SPI_TxRx(SD_DUMMY_BYTE);
    } while ((res & 0x80) && --i);

    return res;
}

static uint8_t SD_ReadData(uint8_t *buff, uint16_t len)
{
    uint16_t i;
    uint32_t timeout = 200;
    uint8_t token;

    do
    {
        token = SD_SPI_TxRx(SD_DUMMY_BYTE);
        if (token == 0xFE) break;
        Delay_ms(1);
    } while (timeout--);

    if (token != 0xFE) return 0;

    for (i = 0; i < len; i++)
    {
        buff[i] = SD_SPI_TxRx(SD_DUMMY_BYTE);
    }

    SD_SPI_TxRx(SD_DUMMY_BYTE);
    SD_SPI_TxRx(SD_DUMMY_BYTE);

    return 1;
}

static uint8_t SD_WriteData(const uint8_t *buff, uint8_t token)
{
    uint16_t i;
    uint8_t resp;

    if (!SD_WaitReady(200)) return 0;

    SD_SPI_TxRx(token);
    if (token == 0xFD) return 1;

    for (i = 0; i < 512; i++)
    {
        SD_SPI_TxRx(buff[i]);
    }

    SD_SPI_TxRx(0xFF);
    SD_SPI_TxRx(0xFF);

    resp = SD_SPI_TxRx(SD_DUMMY_BYTE);
    if ((resp & 0x1F) != 0x05) return 0;

    if (!SD_WaitReady(500)) return 0;
    return 1;
}

static uint8_t SD_ReadCSD(uint8_t *csd)
{
    if (SD_SendCmd(SD_CMD9, 0) != 0) return 0;
    if (!SD_ReadData(csd, 16)) return 0;
    SD_Deselect();
    return 1;
}

static void SD_UpdateSectorCount(void)
{
    uint8_t csd[16];
    uint8_t csdVer;

    s_sectorCount = 0;
    if (!SD_ReadCSD(csd)) return;

    csdVer = (csd[0] >> 6) & 0x03;
    if (csdVer == 1)
    {
        uint32_t csize = ((uint32_t)(csd[7] & 0x3F) << 16) |
                         ((uint32_t)csd[8] << 8) |
                         csd[9];
        s_sectorCount = (csize + 1U) * 1024U;
    }
    else
    {
        uint32_t csize = ((uint32_t)(csd[6] & 0x03) << 10) |
                         ((uint32_t)csd[7] << 2) |
                         ((csd[8] >> 6) & 0x03);
        uint32_t csizeMult = ((uint32_t)(csd[9] & 0x03) << 1) |
                             ((csd[10] >> 7) & 0x01);
        uint32_t readBlLen = csd[5] & 0x0F;
        uint32_t blockNr = (csize + 1U) << (csizeMult + 2U);
        uint32_t blockLen = 1UL << readBlLen;
        s_sectorCount = (blockNr * blockLen) / 512U;
    }
}

void SD_SPI_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    gpio.GPIO_Pin = SD_SPI_PIN_SCK | SD_SPI_PIN_MOSI;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SD_SPI_PORT, &gpio);

    gpio.GPIO_Pin = SD_SPI_PIN_MISO;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SD_SPI_PORT, &gpio);

    gpio.GPIO_Pin = SD_SPI_PIN_CS;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SD_SPI_PORT, &gpio);

    SD_CS_HIGH();

    SPI_I2S_DeInit(SPI1);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_High;
    spi.SPI_CPHA = SPI_CPHA_2Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);
}

uint8_t SD_Card_Init(void)
{
    uint8_t i;
    uint8_t r1;
    uint8_t ocr[4];
    uint32_t retry;

    s_sdReady = 0;
    s_sdHighCapacity = 0;
    s_sectorCount = 0;

    SD_SPI_Init();
    SD_SPI_SetSpeedLow();
    SD_Deselect();

    for (i = 0; i < 10; i++) SD_SPI_TxRx(SD_DUMMY_BYTE);

    r1 = SD_SendCmd(SD_CMD0, 0);
    if (r1 != 1)
    {
        SD_Deselect();
        return 0;
    }

    r1 = SD_SendCmd(SD_CMD8, 0x1AA);
    if (r1 == 1)
    {
        ocr[0] = SD_SPI_TxRx(SD_DUMMY_BYTE);
        ocr[1] = SD_SPI_TxRx(SD_DUMMY_BYTE);
        ocr[2] = SD_SPI_TxRx(SD_DUMMY_BYTE);
        ocr[3] = SD_SPI_TxRx(SD_DUMMY_BYTE);

        if (ocr[2] == 0x01 && ocr[3] == 0xAA)
        {
            retry = 1000;
            do
            {
                r1 = SD_SendCmd(SD_ACMD41, 1UL << 30);
                Delay_ms(1);
            } while (r1 && --retry);

            if (retry && SD_SendCmd(SD_CMD58, 0) == 0)
            {
                ocr[0] = SD_SPI_TxRx(SD_DUMMY_BYTE);
                ocr[1] = SD_SPI_TxRx(SD_DUMMY_BYTE);
                ocr[2] = SD_SPI_TxRx(SD_DUMMY_BYTE);
                ocr[3] = SD_SPI_TxRx(SD_DUMMY_BYTE);
                s_sdHighCapacity = (ocr[0] & 0x40) ? 1 : 0;
                s_sdReady = 1;
            }
        }
    }
    else
    {
        retry = 1000;
        do
        {
            r1 = SD_SendCmd(SD_ACMD41, 0);
            Delay_ms(1);
        } while (r1 && --retry);

        if (!retry)
        {
            retry = 1000;
            do
            {
                r1 = SD_SendCmd(SD_CMD1, 0);
                Delay_ms(1);
            } while (r1 && --retry);
        }

        if (retry && SD_SendCmd(SD_CMD16, 512) == 0)
        {
            s_sdHighCapacity = 0;
            s_sdReady = 1;
        }
    }

    SD_Deselect();

    if (s_sdReady)
    {
        SD_SPI_SetSpeedHigh();
        SD_UpdateSectorCount();
        if (s_sectorCount == 0) s_sectorCount = 32768;
    }

    return s_sdReady;
}

uint8_t SD_Card_GetStatus(void)
{
    return s_sdReady ? 0 : 1;
}

uint8_t SD_Card_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count)
{
    uint32_t i;
    uint32_t addr;

    if (!s_sdReady || count == 0) return 0;

    for (i = 0; i < count; i++)
    {
        addr = s_sdHighCapacity ? (sector + i) : ((sector + i) * 512UL);
        if (SD_SendCmd(SD_CMD17, addr) != 0)
        {
            SD_Deselect();
            return 0;
        }
        if (!SD_ReadData(buff + i * 512U, 512))
        {
            SD_Deselect();
            return 0;
        }
        SD_Deselect();
    }

    return 1;
}

uint8_t SD_Card_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count)
{
    uint32_t i;
    uint32_t addr;

    if (!s_sdReady || count == 0) return 0;

    for (i = 0; i < count; i++)
    {
        addr = s_sdHighCapacity ? (sector + i) : ((sector + i) * 512UL);
        if (SD_SendCmd(SD_CMD24, addr) != 0)
        {
            SD_Deselect();
            return 0;
        }
        if (!SD_WriteData(buff + i * 512U, 0xFE))
        {
            SD_Deselect();
            return 0;
        }
        SD_Deselect();
    }

    return 1;
}

uint32_t SD_Card_GetSectorCount(void)
{
    return s_sectorCount;
}

uint16_t SD_Card_GetSectorSize(void)
{
    return 512;
}
