#ifndef __SD_SPI_H
#define __SD_SPI_H

#include "stm32f10x.h"

void SD_SPI_Init(void);
uint8_t SD_Card_Init(void);
uint8_t SD_Card_GetStatus(void);
uint8_t SD_Card_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count);
uint8_t SD_Card_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count);
uint32_t SD_Card_GetSectorCount(void);
uint16_t SD_Card_GetSectorSize(void);

#endif
