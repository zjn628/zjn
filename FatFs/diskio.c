#include "ff.h"
#include "diskio.h"
#include "stm32f10x.h"

/* ================== 硬件抽象层 ================== */
#define SD_CS_HIGH()    GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define SD_CS_LOW()     GPIO_ResetBits(GPIOA, GPIO_Pin_4)

/* SD命令 */
#define CMD0    (0)
#define CMD8    (8)
#define CMD12   (12)
#define CMD17   (17)
#define CMD24   (24)
#define CMD55   (55)
#define ACMD41  (41)
#define CMD58   (58)

/* ================== SPI 底层 ================== */
static void SPIx_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    // PA4 CS 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);  // 已修复
    SD_CS_HIGH();

    // PA5(SCK) PA7(MOSI) 复用推挽
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);  // 已修复

    // PA6(MISO) 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);  // 已修复

    // 标准 SPI 模式0
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;      
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;    
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE);
}

static uint8_t SPI_ReadWriteByte(uint8_t TxData)
{
    uint8_t retry = 0;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        if(++retry > 200) return 0xFF;
    }
    SPI_I2S_SendData(SPI1, TxData);
    retry = 0;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        if(++retry > 200) return 0xFF;
    }
    return SPI_I2S_ReceiveData(SPI1);
}

static void SPI_SetSpeed(uint16_t prescaler)
{
    SPI1->CR1 &= ~(0x38);
    SPI1->CR1 |= (prescaler & 0x38);
}

/* ================== SD卡基础通讯 ================== */
static void SD_DisSelect(void) {
    SD_CS_HIGH();
    SPI_ReadWriteByte(0xFF);
}

static uint8_t SD_Select(void) {
    SD_CS_LOW();
    if(SPI_ReadWriteByte(0xFF) == 0xFF) return 0;
    SD_DisSelect();
    return 1;
}

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t r1, retry;
    
    SD_DisSelect();
    if(SD_Select()) return 0xFF;

    SPI_ReadWriteByte(0x40 | cmd);
    SPI_ReadWriteByte((uint8_t)(arg >> 24));
    SPI_ReadWriteByte((uint8_t)(arg >> 16));
    SPI_ReadWriteByte((uint8_t)(arg >> 8));
    SPI_ReadWriteByte((uint8_t)arg);
    SPI_ReadWriteByte(crc);

    if(cmd == CMD12) SPI_ReadWriteByte(0xFF);

    retry = 0;
    do {
        r1 = SPI_ReadWriteByte(0xFF);
    } while((r1 & 0x80) && ++retry < 200);

    return r1;
}

/* ================== FATFS 接口 ================== */
static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv) {
    if(pdrv) return STA_NOINIT;
    return Stat;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    uint8_t r1, buf[4];
    uint16_t retry;
    if(pdrv != 0) return STA_NOINIT;

    SPIx_Init();
    
    for(uint8_t i=0; i<15; i++) SPI_ReadWriteByte(0xFF);

    retry = 0;
    do {
        r1 = SD_SendCmd(CMD0, 0, 0x95);
    } while((r1 != 0x01) && ++retry < 200);

    if(r1 != 0x01) return STA_NOINIT;

    if(SD_SendCmd(CMD8, 0x1AA, 0x87) == 0x01) {
        for(uint8_t i=0; i<4; i++) buf[i] = SPI_ReadWriteByte(0xFF);
        retry = 0;
        do {
            SD_SendCmd(CMD55, 0, 0x01);
            r1 = SD_SendCmd(ACMD41, 0x40000000, 0x01);
        } while(r1 && ++retry < 1000);
    } else {
        SD_SendCmd(CMD55, 0, 0x01);
        r1 = SD_SendCmd(ACMD41, 0, 0x01);
        if(r1 <= 1) {
            retry = 0;
            do {
                SD_SendCmd(CMD55, 0, 0x01);
                r1 = SD_SendCmd(ACMD41, 0, 0x01);
            } while(r1 && ++retry < 1000);
        } else {
            retry = 0;
            do {
                r1 = SD_SendCmd(1, 0, 0x01);
            } while(r1 && ++retry < 1000);
        }
    }

    if(r1 == 0) {
        Stat &= ~STA_NOINIT;
        SPI_SetSpeed(SPI_BaudRatePrescaler_4); 
    } else {
        Stat |= STA_NOINIT;
    }
    
    SD_DisSelect();
    return Stat;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if(pdrv || !count || (Stat & STA_NOINIT)) return RES_PARERR;

    uint32_t addr = sector << 9; 

    while(count--) {
        if(SD_SendCmd(CMD17, addr, 0x01) != 0x00) {
            SD_DisSelect();
            return RES_ERROR;
        }

        uint32_t timeout = 0x1FFFF;
        while(SPI_ReadWriteByte(0xFF) != 0xFE) {
            if(--timeout == 0) {
                SD_DisSelect();
                return RES_ERROR;
            }
        }

        for(uint16_t i=0; i<512; i++) {
            buff[i] = SPI_ReadWriteByte(0xFF);
        }
        
        SPI_ReadWriteByte(0xFF);
        SPI_ReadWriteByte(0xFF);
        
        addr += 512;
        buff += 512;
        SD_DisSelect();
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if(pdrv || !count || (Stat & STA_NOINIT)) return RES_PARERR;

    uint32_t addr = sector << 9;

    while(count--) {
        if(SD_SendCmd(CMD24, addr, 0x01) != 0) {
            SD_DisSelect();
            return RES_ERROR;
        }

        SPI_ReadWriteByte(0xFF);
        SPI_ReadWriteByte(0xFE);
        
        for(uint16_t i=0; i<512; i++) {
            SPI_ReadWriteByte(buff[i]);
        }
        
        SPI_ReadWriteByte(0xFF);
        SPI_ReadWriteByte(0xFF);

        uint8_t resp = SPI_ReadWriteByte(0xFF);
        if((resp & 0x1F) != 0x05) {
            SD_DisSelect();
            return RES_ERROR;
        }
        
        uint32_t timeout = 0x1FFFF;
        while(SPI_ReadWriteByte(0xFF) == 0) {
            if(--timeout == 0) break;
        }
        
        addr += 512;
        buff += 512;
        SD_DisSelect();
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    if(pdrv != 0) return RES_PARERR;
    if(Stat & STA_NOINIT) return RES_NOTRDY;

    switch(cmd) {
        case CTRL_SYNC: return RES_OK;
        case GET_SECTOR_COUNT: return RES_ERROR;
        case GET_SECTOR_SIZE: *(WORD*)buff = 512; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD*)buff = 1; return RES_OK;
    }
    return RES_PARERR;
}

DWORD get_fattime (void)
{
    return ((DWORD)(2026 - 1980) << 25) | (4 << 21) | (23 << 16) | (22 << 11) | (0 << 5) | 0;
}