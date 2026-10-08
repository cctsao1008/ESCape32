/*
** AM13E PB14 single-wire ESCape32 boot transport.
** 38400 baud, 8N1, open-drain-style GPIO (drive low, release high).
** MCLK/SysTick source: reset-default 32 MHz SYSOSC.
*/
#include "common.h"
#include <dl_gpio.h>
#include <dl_systick.h>
#include <hw_pinmap.h>

/* PB14 is GPIO46: GPIO1, bit 14 (46 - 32). */
#define BOOT_IO_PORT GPIO1
#define BOOT_IO_MASK (UINT32_C(1) << 14)
#define BOOT_TICK_HZ UINT32_C(32000000)
#define BOOT_BIT_TICKS ((BOOT_TICK_HZ + 19200U) / 38400U)
#define BOOT_TIMEOUT_TICKS (BOOT_TICK_HZ / 2U)
#define BOOT_TICK_MASK UINT32_C(0x00ffffff)

static uint32_t elapsed(uint32_t start) {
    return (start - DL_SYSTICK_getValue()) & BOOT_TICK_MASK;
}

static void wait_ticks(uint32_t ticks) {
    uint32_t start = DL_SYSTICK_getValue();
    while (elapsed(start) < ticks) {}
}

static bool level(void) {
    return (DL_GPIO_readPins(BOOT_IO_PORT, BOOT_IO_MASK) != 0U);
}

/* Keep output latch low. Releasing the line tri-states the GPIO. */
static void drive_low(void) {
    DL_GPIO_enableOutput(BOOT_IO_PORT, BOOT_IO_MASK);
}

static void release_line(void) {
    DL_GPIO_disableOutput(BOOT_IO_PORT, BOOT_IO_MASK);
}

void initio(void) {
    DL_GPIO_enablePower(BOOT_IO_PORT);
    DL_GPIO_initDigitalInput(IOMUX_PINCM_PB14);
    DL_GPIO_clearPins(BOOT_IO_PORT, BOOT_IO_MASK);
    release_line();
    DL_SYSTICK_init(BOOT_TICK_MASK + 1U);
    DL_SYSTICK_enable();
}

int recvbuf(char *buf, int len) {
    if (!buf || len < 0) return 0;
    for (int i = 0; i < len; ++i) {
        uint32_t start = DL_SYSTICK_getValue();
        while (level()) {
            if (elapsed(start) >= BOOT_TIMEOUT_TICKS) return 0;
        }
        wait_ticks(BOOT_BIT_TICKS / 2U);
        if (level()) return 0; /* Reject false start. */
        uint8_t val = 0;
        for (unsigned bit = 0; bit < 8U; ++bit) {
            wait_ticks(BOOT_BIT_TICKS);
            if (level()) val |= (uint8_t)(1U << bit);
        }
        wait_ticks(BOOT_BIT_TICKS);
        if (!level()) return 0; /* Stop bit. */
        buf[i] = (char)val;
    }
    return 1;
}

void sendbuf(const char *buf, int len) {
    if (!buf || len <= 0) return;
    for (int i = 0; i < len; ++i) {
        uint8_t val = (uint8_t)buf[i];
        drive_low(); /* Start bit. */
        wait_ticks(BOOT_BIT_TICKS);
        for (unsigned bit = 0; bit < 8U; ++bit) {
            if (val & (uint8_t)(1U << bit)) release_line();
            else drive_low();
            wait_ticks(BOOT_BIT_TICKS);
        }
        release_line(); /* Stop bit. */
        wait_ticks(BOOT_BIT_TICKS);
    }
}
