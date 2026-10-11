/* TI AM13E BCR NONMAIN static write/erase protection, Rel17 setwrp
 * Off=0 Boot=1 Full=2. This is a DATA PLANNER, never flash I/O.
 * TRM SPRUJF2B 5.3/5.4 and 3.17.43..3.17.45, 13.4:
 * BANK0 NM1 0x60100800, physical 2048B, user config offset 0xe4,
 * protect MAIN A at 0xe8, MAIN B at 0xec.
 * The entire BCR+BSL region must be preserved byte-for-byte except
 * those two policy words. A hardware BSL/SWD recovery test is REQUIRED.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define AM13E_BOOT_NONMAIN_CFG_ADDR UINT32_C(0x60100800)
#define AM13E_BOOT_NONMAIN_CFG_BYTES 2048U
#define AM13E_BOOT_NONMAIN_WEP_CONFIG_OFFSET 0xe4U
#define AM13E_BOOT_NONMAIN_MAIN_A_OFFSET 0xe8U
#define AM13E_BOOT_NONMAIN_MAIN_B_OFFSET 0xecU
#define AM13E_BOOT_NONMAIN_WEP_DISABLE UINT16_C(0xaabb)
typedef enum {
  AM13E_WRP_OFF=0,AM13E_WRP_BOOT=1,AM13E_WRP_FULL=2
} AM13E_NonmainWrpMode;
/* Original Rel17 command entry; Reference is always a no-op + NAK. */
void setwrp(int type);
bool boot_am13e_nonmain_wrp_prepare(const uint8_t *original,
    unsigned length,AM13E_NonmainWrpMode mode,uint8_t *planned,
    unsigned out_bytes,bool *needs_write);
