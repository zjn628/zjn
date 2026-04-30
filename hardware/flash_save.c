#include "stm32f10x_flash.h"
#define SAVE_ADDR 0x0800FC00 // 最后一页地址

void Save_GDD_To_Flash(float gdd) {
    FLASH_Unlock();
    FLASH_ErasePage(SAVE_ADDR);
    uint32_t *p = (uint32_t*)&gdd;
    FLASH_ProgramWord(SAVE_ADDR, *p);
    FLASH_Lock();
}

float Load_GDD_From_Flash(void) {
    uint32_t data = *(__IO uint32_t*)SAVE_ADDR;
    if (data == 0xFFFFFFFF) return 0.0f;
    return *(float*)&data;
}