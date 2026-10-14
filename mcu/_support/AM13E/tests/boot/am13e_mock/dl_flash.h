#pragma once
#include <stdint.h>
#define DL_FLASH_SUCCESS 0U
#define DL_FLASH_ERROR 1U
#define DL_FLASH_SECTOR_SIZE 2048U
uint32_t DL_Flash_eraseSector(uint32_t addr);
uint32_t DL_Flash_program(uint32_t addr, uint8_t *src, uint32_t len);
