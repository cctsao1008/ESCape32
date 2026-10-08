#pragma once
#include <stdint.h>
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t state) { (void)state; }
static inline void __DSB(void) {}
static inline void __ISB(void) {}
