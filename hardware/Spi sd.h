/*-----------------------------------------------------------------------
 * spi_sd.h - SPI底层驱动 + SD卡驱动头文件
 * 硬件：STM32F103C8T6
 * SPI1: SCK=PA5, MISO=PA6, MOSI=PA7, CS=PA4
 *-----------------------------------------------------------------------*/
#ifndef __SPI_SD_H
#define __SPI_SD_H

#include "stm32f10x.h"

/*-----------------------------------------------------------------------
 * 引脚宏定义
 *-----------------------------------------------------------------------*/
#define SD_SPI              SPI1
#define SD_SPI_RCC          RCC_APB2Periph_SPI1
#define SD_GPIO_RCC         RCC_APB2Periph_GPIOA
#define SD_GPIO             GPIOA

#define SD_SCK_PIN          GPIO_Pin_5
#define SD_MISO_PIN         GPIO_Pin_6
#define SD_MOSI_PIN         GPIO_Pin_7
#define SD_CS_PIN           GPIO_Pin_4

/* CS引脚控制 */
#define SD_CS_LOW()         GPIO_ResetBits(SD_GPIO, SD_CS_PIN)
#define SD_CS_HIGH()        GPIO_SetBits(SD_GPIO, SD_CS_PIN)

/*-----------------------------------------------------------------------
 * SD卡命令宏（CMD帧格式：0x40|CMD, 4字节参数, CRC）
 *-----------------------------------------------------------------------*/
#define CMD0    (0x40 + 0)   /* GO_IDLE_STATE：复位 */
#define CMD1    (0x40 + 1)   /* SEND_OP_COND：MMC初始化 */
#define CMD8    (0x40 + 8)   /* SEND_IF_COND：检测SD V2 */
#define CMD9    (0x40 + 9)   /* SEND_CSD：读CSD寄存器 */
#define CMD10   (0x40 + 10)  /* SEND_CID：读CID寄存器 */
#define CMD12   (0x40 + 12)  /* STOP_TRANSMISSION：停止多块传输 */
#define CMD16   (0x40 + 16)  /* SET_BLOCKLEN：设置块长度 */
#define CMD17   (0x40 + 17)  /* READ_SINGLE_BLOCK：读单块 */
#define CMD18   (0x40 + 18)  /* READ_MULTIPLE_BLOCK：读多块 */
#define CMD23   (0x40 + 23)  /* SET_BLOCK_COUNT：MMC预设块数 */
#define CMD24   (0x40 + 24)  /* WRITE_BLOCK：写单块 */
#define CMD25   (0x40 + 25)  /* WRITE_MULTIPLE_BLOCK：写多块 */
#define CMD41   (0x40 + 41)  /* APP_SEND_OP_COND：SD初始化 */
#define CMD55   (0x40 + 55)  /* APP_CMD：应用命令前缀 */
#define CMD58   (0x40 + 58)  /* READ_OCR：读OCR寄存器 */

/* SD卡类型标志 */
#define SD_TYPE_ERR     0x00    /* 错误/未识别 */
#define SD_TYPE_MMC     0x01    /* MMC卡 */
#define SD_TYPE_V1      0x02    /* SD V1 */
#define SD_TYPE_V2      0x04    /* SD V2 */
#define SD_TYPE_HC      0x06    /* SDHC/SDXC（块地址） */

/* 数据令牌 */
#define SD_TOKEN_START_BLOCK    0xFE    /* 单块读/写数据令牌 */
#define SD_TOKEN_START_MULTI    0xFC    /* 多块写数据令牌 */
#define SD_TOKEN_STOP_MULTI     0xFD    /* 多块写停止令牌 */

extern uint8_t SD_Type;     /* 当前卡类型（全局变量） */

/*-----------------------------------------------------------------------
 * SPI底层接口
 *-----------------------------------------------------------------------*/
void    SPI_SD_Init(void);              /* SPI1初始化（低速400kHz） */
void    SPI_SetSpeed(uint8_t fast);     /* 切换SPI速度：1=高速, 0=低速 */
uint8_t SPI_ReadWriteByte(uint8_t dat); /* SPI收发一个字节 */

/*-----------------------------------------------------------------------
 * SD卡接口
 *-----------------------------------------------------------------------*/
uint8_t SD_Init(void);                  /* SD卡初始化，返回0=成功 */
uint8_t SD_ReadBlock(uint8_t *buf, uint32_t sector);   /* 读一个512字节扇区 */
uint8_t SD_WriteBlock(const uint8_t *buf, uint32_t sector); /* 写一个512字节扇区 */
uint8_t SD_GetSectorCount(uint32_t *count); /* 获取总扇区数 */

#endif /* __SPI_SD_H */