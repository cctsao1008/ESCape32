/*
 * ESCape32 Rel17 AM13E Application MCU configuration.
 *
 * This file is intentionally independent of boot/mcu/AM13E/config.h.
 * Stage E1-A only establishes the include/platform boundary.
 *
 * Board-dependent clock, PWM dead-time, commutation timing, COMP/ADC
 * routing, pin multiplexing and gate-driver output polarity are UNKNOWN.
 * Do not define fabricated STM32-style TIM/USART/DMA register aliases
 * or dummy hardware constants merely to make the application compile.
 */
#pragma once

#ifndef AM13E
#error "AM13E Application configuration included without AM13E"
#endif
