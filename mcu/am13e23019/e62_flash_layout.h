/*
 * E62 AM13E23019 MAIN Flash contract.
 *
 * Source of truth: E62 AM13E23019 SW Architecture Baseline v1.4.
 */
#pragma once
#include <stdint.h>

#define E62_FLASH_BASE            (0x00000000UL)
#define E62_FLASH_END             (0x00080000UL)
#define E62_BOOT_BASE             (0x00000000UL)
#define E62_BOOT_SIZE             (0x00004000UL)
#define E62_BOOT_END              (E62_BOOT_BASE + E62_BOOT_SIZE)
#define E62_FW1_CFG_BASE          (0x00004000UL)
#define E62_FW1_CFG_SIZE          (0x00001000UL)
#define E62_FW1_CFG_END           (E62_FW1_CFG_BASE + E62_FW1_CFG_SIZE)
#define E62_FW2_CFG_BASE          (0x00005000UL)
#define E62_FW2_CFG_SIZE          (0x00001000UL)
#define E62_FW2_CFG_END           (E62_FW2_CFG_BASE + E62_FW2_CFG_SIZE)
#define E62_APP_BASE              (0x00006000UL)
#define E62_APP_END               (0x00080000UL)
#define E62_APP_SIZE              (E62_APP_END - E62_APP_BASE)
#define E62_FLASH_BANK0_BASE      (0x00000000UL)
#define E62_FLASH_BANK0_END       (0x00040000UL)
#define E62_FLASH_BANK1_BASE      (0x00040000UL)
#define E62_FLASH_BANK1_END       (0x00080000UL)
#define E62_FLASH_SECTOR_SIZE     (0x00000800UL)
#define E62_RAM_S_BASE            (0x20000000UL)
#define E62_RAM_S_SIZE            (0x00018000UL)
#define E62_RAM_S_END             (E62_RAM_S_BASE + E62_RAM_S_SIZE)

#if (E62_BOOT_END != E62_FW1_CFG_BASE)
#error "E62 flash layout gap/overlap between boot and FW1 CFG"
#endif
#if (E62_FW1_CFG_END != E62_FW2_CFG_BASE)
#error "E62 flash layout gap/overlap between FW1 CFG and FW2 CFG"
#endif
#if (E62_FW2_CFG_END != E62_APP_BASE)
#error "E62 flash layout gap/overlap between FW2 CFG and APP"
#endif
#if (E62_APP_END != E62_FLASH_END)
#error "E62 application region must terminate at end of MAIN Flash"
#endif
#if ((E62_BOOT_SIZE % E62_FLASH_SECTOR_SIZE) != 0)
#error "E62 boot region must be sector aligned"
#endif
#if ((E62_FW1_CFG_SIZE % E62_FLASH_SECTOR_SIZE) != 0)
#error "E62 FW1 CFG region must be sector aligned"
#endif
#if ((E62_FW2_CFG_SIZE % E62_FLASH_SECTOR_SIZE) != 0)
#error "E62 FW2 CFG region must be sector aligned"
#endif
