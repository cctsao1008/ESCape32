/*
 * AM13E23019 boot platform contract.
 *
 * Boot uses a deterministic 32-MHz SYSOSC clock basis so the PB14 software
 * service transport does not depend on the application clock configuration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOOT_PLATFORM_MCLK_HZ 32000000U

bool boot_platform_init(void);
