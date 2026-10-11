/*
 * Board-independent ESCape32 Rel17 AM13E runtime timebase policy.
 * This is an application scheduling rate, not a crystal or PLL setting.
 */
#pragma once
#ifndef AM13E
#error "AM13E runtime tick contract must not leak to legacy MCUs"
#endif
#include <stdint.h>
#define AM13E_APP_SYSTICK_HZ UINT32_C(16000)
#define AM13E_APP_SYSTICK_MAX_CYCLES UINT32_C(0x01000000)
