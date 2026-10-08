/*
 * ESCape32 AM13E23019 Flash/image layout.
 *
 * This file describes the software image contract for the AM13E23019 port.
 */
#pragma once
#include <stdint.h>

#define ESCAPE32_FLASH_BASE            (0x00000000UL)
#define ESCAPE32_FLASH_END             (0x00080000UL)
#define ESCAPE32_BOOT_BASE             (0x00000000UL)
#define ESCAPE32_BOOT_SIZE             (0x00004000UL)
#define ESCAPE32_BOOT_END              (ESCAPE32_BOOT_BASE + ESCAPE32_BOOT_SIZE)
#define ESCAPE32_FW1_CFG_BASE          (0x00004000UL)
#define ESCAPE32_FW1_CFG_SIZE          (0x00001000UL)
#define ESCAPE32_FW1_CFG_END           (ESCAPE32_FW1_CFG_BASE + ESCAPE32_FW1_CFG_SIZE)
#define ESCAPE32_FW2_CFG_BASE          (0x00005000UL)
#define ESCAPE32_FW2_CFG_SIZE          (0x00001000UL)
#define ESCAPE32_FW2_CFG_END           (ESCAPE32_FW2_CFG_BASE + ESCAPE32_FW2_CFG_SIZE)
#define ESCAPE32_APP_BASE              (0x00006000UL)
#define ESCAPE32_APP_END               (0x00080000UL)
#define ESCAPE32_APP_SIZE              (ESCAPE32_APP_END - ESCAPE32_APP_BASE)
#define ESCAPE32_FLASH_BANK0_BASE      (0x00000000UL)
#define ESCAPE32_FLASH_BANK0_END       (0x00040000UL)
#define ESCAPE32_FLASH_BANK1_BASE      (0x00040000UL)
#define ESCAPE32_FLASH_BANK1_END       (0x00080000UL)
#define ESCAPE32_FLASH_SECTOR_SIZE     (0x00000800UL)
#define ESCAPE32_RAM_S_BASE            (0x20000000UL)
#define ESCAPE32_RAM_S_SIZE            (0x00018000UL)
#define ESCAPE32_RAM_S_END             (ESCAPE32_RAM_S_BASE + ESCAPE32_RAM_S_SIZE)

#if (ESCAPE32_BOOT_END != ESCAPE32_FW1_CFG_BASE)
#error "Flash layout gap/overlap between boot and FW1 CFG"
#endif
#if (ESCAPE32_FW1_CFG_END != ESCAPE32_FW2_CFG_BASE)
#error "Flash layout gap/overlap between FW1 CFG and FW2 CFG"
#endif
#if (ESCAPE32_FW2_CFG_END != ESCAPE32_APP_BASE)
#error "Flash layout gap/overlap between FW2 CFG and APP"
#endif
#if (ESCAPE32_APP_END != ESCAPE32_FLASH_END)
#error "Application region must terminate at end of MAIN Flash"
#endif
#if ((ESCAPE32_BOOT_SIZE % ESCAPE32_FLASH_SECTOR_SIZE) != 0)
#error "Boot region must be sector aligned"
#endif
#if ((ESCAPE32_FW1_CFG_SIZE % ESCAPE32_FLASH_SECTOR_SIZE) != 0)
#error "FW1 CFG region must be sector aligned"
#endif
#if ((ESCAPE32_FW2_CFG_SIZE % ESCAPE32_FLASH_SECTOR_SIZE) != 0)
#error "FW2 CFG region must be sector aligned"
#endif
