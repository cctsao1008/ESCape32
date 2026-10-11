/*
 * Native AM13E Rel17 Boot self-update Flash commit experiment.
 * The Boot dispatcher deliberately DOES NOT invoke this module:
 * executing a Boot Bank0 erase without qualified ROM BSL/SWD recovery
 * risks an unrecoverable device. Host tests exercise real sector logic.
 */
#pragma once
#include <stdint.h>
typedef enum {
    AM13E_BOOT_COMMIT_OK=0,
    AM13E_BOOT_COMMIT_BAD_IMAGE,
    AM13E_BOOT_COMMIT_ERASE_FAILED,
    AM13E_BOOT_COMMIT_PROGRAM_FAILED,
    AM13E_BOOT_COMMIT_VERIFY_FAILED
} AM13E_BootCommitStatus;

#ifdef AM13E_BOOT_COMMIT_HOST_TEST
/* Host-only test entry: fake Boot base must match the mock Flash region. */
AM13E_BootCommitStatus boot_am13e_update_commit_host_run(uint32_t boot_base);
#else
/* This experiment has NO call site in boot/src/main.c.
 * If ever enabled, this nonreturning wrapper stays in SRAM_C from the
 * first erase through verify and either reset or permanent failure halt.
 */
void boot_am13e_update_commit_quarantined(void)
    __attribute__((noreturn));
#endif
