/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This firmware is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this firmware. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"

#ifndef HALL_MAP
#elif HALL_MAP == 0xAFB35
#define HALL1_PORT A
#define HALL1_PIN1 15
#define HALL2_PORT B
#define HALL2_PIN2 3
#define HALL2_PIN3 5
#elif HALL_MAP == 0xB357
#define HALL_PORT B
#define HALL_PIN1 3
#define HALL_PIN2 5
#define HALL_PIN3 7
#elif HALL_MAP == 0xB358
#define HALL_PORT B
#define HALL_PIN1 3
#define HALL_PIN2 5
#define HALL_PIN3 8
#elif HALL_MAP == 0xB450
#define HALL_PORT B
#define HALL_PIN1 4
#define HALL_PIN2 5
#define HALL_PIN3 0
#endif

#ifndef BEC_MAP
#elif BEC_MAP == 0xA45
#define BEC_PORT A
#define BEC_PIN1 4
#define BEC_PIN2 5
#elif BEC_MAP == 0xB35
#define BEC_PORT B
#define BEC_PIN1 3
#define BEC_PIN2 5
#elif BEC_MAP == 0xCEF
#define BEC_PORT C
#define BEC_PIN1 14
#define BEC_PIN2 15
#endif

#ifndef LED_MAP
#elif LED_MAP == 0xAF
#define LED1_PORT A
#define LED1_PIN 15
#elif LED_MAP == 0xB8
#define LED1_PORT B
#define LED1_PIN 8
#elif LED_MAP == 0xF2AF
#define LED1_PORT F
#define LED1_PIN 2
#define LED2_PORT A
#define LED2_PIN 15
#elif LED_MAP == 0xAFB3B4
#define LED1_PORT A
#define LED1_PIN 15
#define LED2_PORT B
#define LED2_PIN 3
#define LED3_PORT B
#define LED3_PIN 4
#elif LED_MAP == 0xAFB5B3
#define LED1_PORT A
#define LED1_PIN 15
#define LED2_PORT B
#define LED2_PIN 5
#define LED3_PORT B
#define LED3_PIN 3
#elif LED_MAP == 0xB5B3AF
#define LED1_PORT B
#define LED1_PIN 5
#define LED2_PORT B
#define LED2_PIN 3
#define LED3_PORT A
#define LED3_PIN 15
#elif LED_MAP == 0xB5B4B3
#define LED1_PORT B
#define LED1_PIN 5
#define LED2_PORT B
#define LED2_PIN 4
#define LED3_PORT B
#define LED3_PIN 3
#elif LED_MAP == 0xB8B5B3
#define LED1_PORT B
#define LED1_PIN 8
#define LED2_PORT B
#define LED2_PIN 5
#define LED3_PORT B
#define LED3_PIN 3
#endif

#ifdef LED_INV
#define LED1_INV
#define LED2_INV
#define LED3_INV
#endif

#ifdef LED_OD
#define LED1_OD
#define LED2_OD
#define LED3_OD
#endif

#ifndef FLASH_CR_STRT
#define FLASH_CR_STRT FLASH_CR_START
#endif

static char busy;

void initgpio(void) {
#if defined HALL_MAP && !defined USE_XOR
#ifdef HALL1_PORT
#ifdef HALL2_PIN2
	GPIO(HALL1_PORT, PUPDR) |= 1 << HALL1_PIN1 * 2;
	GPIO(HALL2_PORT, PUPDR) |= 1 << HALL2_PIN2 * 2 | 1 << HALL2_PIN3 * 2;
	GPIO(HALL1_PORT, MODER) &= ~(3 << HALL1_PIN1 * 2);
	GPIO(HALL2_PORT, MODER) &= ~(3 << HALL2_PIN2 * 2 | 3 << HALL2_PIN3 * 2);
#else
	GPIO(HALL1_PORT, PUPDR) |= 1 << HALL1_PIN1 * 2 | 1 << HALL1_PIN2 * 2;
	GPIO(HALL2_PORT, PUPDR) |= 1 << HALL2_PIN3 * 2;
	GPIO(HALL1_PORT, MODER) &= ~(3 << HALL1_PIN1 * 2 | 3 << HALL1_PIN2 * 2);
	GPIO(HALL2_PORT, MODER) &= ~(3 << HALL2_PIN3 * 2);
#endif
#else
	GPIO(HALL_PORT, PUPDR) |= 1 << HALL_PIN1 * 2 | 1 << HALL_PIN2 * 2 | 1 << HALL_PIN3 * 2;
	GPIO(HALL_PORT, MODER) &= ~(3 << HALL_PIN1 * 2 | 3 << HALL_PIN2 * 2 | 3 << HALL_PIN3 * 2);
#endif
#endif
#ifdef BEC_MAP
	int x = cfg.bec - BEC_MIN;
#if BEC_MAP == 0xADE // SWD pins
	if (!(GPIOA_IDR & 0x6000)) { // External pull-down
		GPIOA_ODR |= (x & 3) << 13;
		GPIOA_OSPEEDR &= ~0x3c000000; // A13,A14 (low speed)
		GPIOA_PUPDR &= ~0x3c000000; // A13,A14 (no pull-up/pull-down)
		GPIOA_MODER ^= 0x3c000000; // A13,A14 (output)
	}
#else
	int y = GPIO(BEC_PORT, ODR) | (x & 1) << BEC_PIN1;
	int z = GPIO(BEC_PORT, MODER) & ~(2 << BEC_PIN1 * 2);
#ifdef BEC_PIN2
#if BEC_PIN2 >= 1
	y |= (x & 2) << (BEC_PIN2 - 1);
#else
	y |= (x & 2) >> (1 - BEC_PIN2);
#endif
	z &= ~(2 << BEC_PIN2 * 2);
#ifdef BEC_PIN3
#if BEC_PIN3 >= 2
	y |= (x & 4) << (BEC_PIN3 - 2);
#else
	y |= (x & 4) >> (2 - BEC_PIN3);
#endif
	z &= ~(2 << BEC_PIN3 * 2);
#endif
#endif
	GPIO(BEC_PORT, ODR) = y;
	GPIO(BEC_PORT, MODER) = z;
#endif
#endif
#ifdef ERPM_PIN
	GPIO(ERPM_PORT, MODER) = (GPIO(ERPM_PORT, MODER) & ~(3 << ERPM_PIN * 2)) | 1 << ERPM_PIN * 2;
#endif
#ifdef PARK_PIN
	GPIO(PARK_PORT, PUPDR) |= 1 << PARK_PIN * 2;
	GPIO(PARK_PORT, MODER) &= ~(3 << PARK_PIN * 2);
#endif
}

__attribute__((__weak__))
void initled(void) {
#ifdef LED1_PORT
#ifdef LED1_INV
	GPIO(LED1_PORT, ODR) |= 1 << LED1_PIN;
#endif
#ifdef LED1_OD
	GPIO(LED1_PORT, OTYPER) |= 1 << LED1_PIN;
#endif
	GPIO(LED1_PORT, MODER) &= ~(2 << LED1_PIN * 2);
#endif
#ifdef LED2_PORT
#ifdef LED2_INV
	GPIO(LED2_PORT, ODR) |= 1 << LED2_PIN;
#endif
#ifdef LED2_OD
	GPIO(LED2_PORT, OTYPER) |= 1 << LED2_PIN;
#endif
	GPIO(LED2_PORT, MODER) &= ~(2 << LED2_PIN * 2);
#endif
#ifdef LED3_PORT
#ifdef LED3_INV
	GPIO(LED3_PORT, ODR) |= 1 << LED3_PIN;
#endif
#ifdef LED3_OD
	GPIO(LED3_PORT, OTYPER) |= 1 << LED3_PIN;
#endif
	GPIO(LED3_PORT, MODER) &= ~(2 << LED3_PIN * 2);
#endif
#ifdef LED4_PORT
#ifdef LED4_INV
	GPIO(LED4_PORT, ODR) |= 1 << LED4_PIN;
#endif
#ifdef LED4_OD
	GPIO(LED4_PORT, OTYPER) |= 1 << LED4_PIN;
#endif
	GPIO(LED4_PORT, MODER) &= ~(2 << LED4_PIN * 2);
#endif
}

#ifdef HALL_MAP
int hallcode(void) {
#ifdef HALL1_PORT
	int x1 = GPIO(HALL1_PORT, IDR);
	int x2 = GPIO(HALL2_PORT, IDR);
#ifdef HALL2_PIN2
#if HALL2_PIN3 >= 2
#if HALL2_PIN2 >= 1
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x2 & (1 << HALL2_PIN2)) >> (HALL2_PIN2 - 1) | (x2 & (1 << HALL2_PIN3)) >> (HALL2_PIN3 - 2);
#else
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x2 & (1 << HALL2_PIN2)) << (1 - HALL2_PIN2) | (x2 & (1 << HALL2_PIN3)) >> (HALL2_PIN3 - 2);
#endif
#else
#if HALL2_PIN2 >= 1
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x2 & (1 << HALL2_PIN2)) >> (HALL2_PIN2 - 1) | (x2 & (1 << HALL2_PIN3)) << (2 - HALL2_PIN3);
#else
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x2 & (1 << HALL2_PIN2)) << (1 - HALL2_PIN2) | (x2 & (1 << HALL2_PIN3)) << (2 - HALL2_PIN3);
#endif
#endif
#else
#if HALL2_PIN3 >= 2
#if HALL1_PIN2 >= 1
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x1 & (1 << HALL1_PIN2)) >> (HALL1_PIN2 - 1) | (x2 & (1 << HALL2_PIN3)) >> (HALL2_PIN3 - 2);
#else
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x1 & (1 << HALL1_PIN2)) << (1 - HALL1_PIN2) | (x2 & (1 << HALL2_PIN3)) >> (HALL2_PIN3 - 2);
#endif
#else
#if HALL1_PIN2 >= 1
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x1 & (1 << HALL1_PIN2)) >> (HALL1_PIN2 - 1) | (x2 & (1 << HALL2_PIN3)) << (2 - HALL2_PIN3);
#else
	return (x1 & (1 << HALL1_PIN1)) >> HALL1_PIN1 | (x1 & (1 << HALL1_PIN2)) << (1 - HALL1_PIN2) | (x2 & (1 << HALL2_PIN3)) << (2 - HALL2_PIN3);
#endif
#endif
#endif
#else
	int x = GPIO(HALL_PORT, IDR);
#if HALL_PIN3 >= 2
#if HALL_PIN2 >= 1
	return (x & (1 << HALL_PIN1)) >> HALL_PIN1 | (x & (1 << HALL_PIN2)) >> (HALL_PIN2 - 1) | (x & (1 << HALL_PIN3)) >> (HALL_PIN3 - 2);
#else
	return (x & (1 << HALL_PIN1)) >> HALL_PIN1 | (x & (1 << HALL_PIN2)) << (1 - HALL_PIN2) | (x & (1 << HALL_PIN3)) >> (HALL_PIN3 - 2);
#endif
#else
#if HALL_PIN2 >= 1
	return (x & (1 << HALL_PIN1)) >> HALL_PIN1 | (x & (1 << HALL_PIN2)) >> (HALL_PIN2 - 1) | (x & (1 << HALL_PIN3)) << (2 - HALL_PIN3);
#else
	return (x & (1 << HALL_PIN1)) >> HALL_PIN1 | (x & (1 << HALL_PIN2)) << (1 - HALL_PIN2) | (x & (1 << HALL_PIN3)) << (2 - HALL_PIN3);
#endif
#endif
#endif
}
#endif

__attribute__((__weak__))
void ledctl(int x) {
#ifdef LED1_PORT
#ifdef LED1_INV
	GPIO(LED1_PORT, BSRR) = x & 1 ? 1 << (LED1_PIN + 16) : 1 << LED1_PIN;
#else
	GPIO(LED1_PORT, BSRR) = x & 1 ? 1 << LED1_PIN : 1 << (LED1_PIN + 16);
#endif
#endif
#ifdef LED2_PORT
#ifdef LED2_INV
	GPIO(LED2_PORT, BSRR) = x & 2 ? 1 << (LED2_PIN + 16) : 1 << LED2_PIN;
#else
	GPIO(LED2_PORT, BSRR) = x & 2 ? 1 << LED2_PIN : 1 << (LED2_PIN + 16);
#endif
#endif
#ifdef LED3_PORT
#ifdef LED3_INV
	GPIO(LED3_PORT, BSRR) = x & 4 ? 1 << (LED3_PIN + 16) : 1 << LED3_PIN;
#else
	GPIO(LED3_PORT, BSRR) = x & 4 ? 1 << LED3_PIN : 1 << (LED3_PIN + 16);
#endif
#endif
#ifdef LED4_PORT
#ifdef LED4_INV
	GPIO(LED4_PORT, BSRR) = x & 8 ? 1 << (LED4_PIN + 16) : 1 << LED4_PIN;
#else
	GPIO(LED4_PORT, BSRR) = x & 8 ? 1 << LED4_PIN : 1 << (LED4_PIN + 16);
#endif
#endif
}

__attribute__((__weak__))
void hsictl(int x) {
	int cr = RCC_CR;
	int tv = (cr & 0xf8) >> 3; // 5 bits
	RCC_CR = (cr & ~0xf8) | clamp(tv + x, 0, 0x1f) << 3;
}

void checkcfg(void) {
#ifndef ANALOG
#ifndef ANALOG_CHAN
	if (IO_ANALOG) cfg.arm = 1; // Ensure low level on startup
	else
#endif
	cfg.arm = !!cfg.arm;
#else
	cfg.arm = 0;
#endif
#ifdef PWM_ENABLE
	cfg.damp = 1;
#else
	cfg.damp = !!cfg.damp;
#endif
	cfg.revdir = !!cfg.revdir;
	cfg.brushed = !!cfg.brushed;
	cfg.timing = clamp(cfg.timing, 1, 31);
	cfg.sine_range = cfg.sine_range && !cfg.brushed ? clamp(cfg.sine_range, 5, 25) : 0;
	cfg.sine_power = clamp(cfg.sine_power, 1, 15);
	cfg.freq_min = clamp(cfg.freq_min, 16, 48);
	cfg.freq_max = clamp(cfg.freq_max, cfg.freq_min, 96);
	cfg.duty_min = clamp(cfg.duty_min, 1, 100);
	cfg.duty_max = clamp(cfg.duty_max, cfg.duty_min, 100);
	cfg.duty_spup = clamp(cfg.duty_spup, 1, 100);
	cfg.duty_ramp = clamp(cfg.duty_ramp, 0, 100);
	cfg.duty_rate = clamp(cfg.duty_rate, 1, 100);
	cfg.duty_drag = clamp(cfg.duty_drag, 0, 100);
	cfg.duty_lock = clamp(cfg.duty_lock, 0, cfg.brushed ? 0 : 2);
	cfg.throt_mode = clamp(cfg.throt_mode, 0, IO_ANALOG ? 0 : cfg.duty_lock ? 1 : 3);
	cfg.throt_rev = clamp(cfg.throt_rev, 0, 3);
	cfg.throt_brk = clamp(cfg.throt_brk, cfg.duty_drag, 100);
	cfg.throt_set = clamp(cfg.throt_set, 0, cfg.arm ? 0 : 100);
	cfg.throt_ztc = !!cfg.throt_ztc;
	cfg.throt_cal = !!cfg.throt_cal;
	cfg.throt_min = clamp(cfg.throt_min, 900, 1900);
	cfg.throt_max = clamp(cfg.throt_max, cfg.throt_min + 200, 2100);
	cfg.throt_mid = clamp(cfg.throt_mid, cfg.throt_min + 100, cfg.throt_max - 100);
	cfg.analog_min = clamp(cfg.analog_min, 0, 3200);
	cfg.analog_max = clamp(cfg.analog_max, cfg.analog_min + 200, 3400);
#ifdef IO_PA2
	cfg.input_mode = clamp(cfg.input_mode, 0, 7);
#ifdef DISABLE_EXBUS
	if (cfg.input_mode == 6) cfg.input_mode = 0;
#endif
#ifdef DISABLE_HOTT
	if (cfg.input_mode == 7) cfg.input_mode = 0;
#endif
	cfg.input_ch1 = clamp(cfg.input_ch1, 1, cfg.input_mode < 3 ? 0 : 32);
	cfg.input_ch2 = clamp(cfg.input_ch2, 0, cfg.input_mode < 3 ? 0 : 32);
#else
#if defined IO_PA6 || defined ANALOG_CHAN
	cfg.input_mode = clamp(cfg.input_mode, 0, 1);
#else
	cfg.input_mode = 0;
#endif
	cfg.input_ch1 = 0;
	cfg.input_ch2 = 0;
#endif
	cfg.telem_mode = clamp(cfg.telem_mode, 0, 6);
#ifdef DISABLE_MSB
	if (cfg.telem_mode == 5) cfg.telem_mode = 0;
#endif
#ifdef DISABLE_HOTT
	if (cfg.telem_mode == 6) cfg.telem_mode = 0;
#endif
	cfg.telem_phid =
		cfg.telem_mode == 2 ||
		cfg.telem_mode == 5 ? clamp(cfg.telem_phid, 1, 2):
		cfg.telem_mode == 3 ? clamp(cfg.telem_phid, 1, 28):
		cfg.telem_mode == 4 ? clamp(cfg.telem_phid, 1, 8):
		cfg.input_mode == 4 ? clamp(cfg.telem_phid, 0, 4) : 0;
	cfg.telem_poles = clamp(cfg.telem_poles & ~1, 2, 100);
#if SENS_CNT >= 1
	cfg.telem_volt = clamp(cfg.telem_volt, -80, 160);
#else
	cfg.telem_volt = 0;
#endif
#if SENS_CNT >= 2
	cfg.telem_curr = clamp(cfg.telem_curr, -100, 200);
#else
	cfg.telem_curr = 0;
#endif
	cfg.prot_stall = cfg.prot_stall && !cfg.brushed ? clamp(cfg.prot_stall, 1500, 3500) : 0;
	cfg.prot_temp = cfg.prot_temp ? clamp(cfg.prot_temp, 60, 140) : 0;
#if SENS_CNT >= 3
	cfg.prot_sens = clamp(cfg.prot_sens, 0, 2);
#else
	cfg.prot_sens = 0;
#endif
#if SENS_CNT >= 1 && !defined ANALOG
	cfg.prot_volt = cfg.prot_volt ? clamp(cfg.prot_volt, 28, 38) : 0;
	cfg.prot_cells = clamp(cfg.prot_cells, 0, 24);
#else
	cfg.prot_volt = 0;
	cfg.prot_cells = 0;
#endif
#if SENS_CNT >= 2
	cfg.prot_curr = clamp(cfg.prot_curr, 0, 999);
#else
	cfg.prot_curr = 0;
#endif
#ifdef PARK_PIN
	cfg.prot_park = clamp(cfg.prot_park, 0, 4);
#else
	cfg.prot_park = 0;
#endif
	cfg.volume = clamp(cfg.volume, 0, 100);
	cfg.beacon = clamp(cfg.beacon, 0, 100);
#ifdef BEC_MAP
	cfg.bec = clamp(cfg.bec, BEC_MIN, BEC_MAX);
#else
	cfg.bec = 0;
#endif
	cfg.led &= (1 << LED_CNT) - 1;
}

int savecfg(void) {
	if (ertm || busy) return 0;
	__disable_irq();
	FLASH_KEYR = FLASH_KEYR_KEY1;
	FLASH_KEYR = FLASH_KEYR_KEY2;
	FLASH_SR = -1; // Clear errors
	FLASH_CR = FLASH_CR_PER;
#ifdef STM32F0
	FLASH_AR = (uint32_t)_cfg;
	FLASH_CR = FLASH_CR_PER | FLASH_CR_STRT; // Erase page
#else
	FLASH_CR = FLASH_CR_PER | FLASH_CR_STRT | ((uint32_t)(_cfg - _boot) >> 11) << FLASH_CR_PNB_SHIFT; // Erase page
#endif
	while (FLASH_SR & FLASH_SR_BSY);
	FLASH_CR = FLASH_CR_PG;
#ifdef STM32F0
#define T uint16_t
#else
#define T uint32_t
#endif
	T *dst = (T *)_cfg;
	T *src = (T *)_cfg_start;
	T *end = (T *)_cfg_end;
#undef T
	while (src < end) { // Write data
		*dst++ = *src++;
#ifndef STM32F0
		*dst++ = *src++;
#endif
		while (FLASH_SR & FLASH_SR_BSY);
	}
	FLASH_CR = FLASH_CR_LOCK;
	__enable_irq();
#ifdef STM32F0
	if (FLASH_SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) return 0;
#else
	if (FLASH_SR & (FLASH_SR_PROGERR | FLASH_SR_WRPERR)) return 0;
#endif
	return !memcmp(_cfg, _cfg_start, _cfg_end - _cfg_start);
}

int resetcfg(void) {
	if (ertm || busy) return 0;
	__disable_irq();
	memcpy(&cfg, &cfgdata, sizeof cfgdata);
	__enable_irq();
	checkcfg();
	rearm = 1; // Ensure safe throttle mode change
	return savecfg();
}

void resetcom(void) {
#ifdef PWM_ENABLE
	TIM1_CCMR1 = TIM_CCMR1_OC1M_FORCE_HIGH | TIM_CCMR1_OC2M_FORCE_HIGH;
	TIM1_CCMR2 = TIM_CCMR2_OC3M_FORCE_HIGH;
	int er = TIM_CCER_CC1NE | TIM_CCER_CC1NP | TIM_CCER_CC2NE | TIM_CCER_CC2NP | TIM_CCER_CC3NE | TIM_CCER_CC3NP;
#else
	TIM1_CCMR1 = TIM_CCMR1_OC1M_FORCE_LOW | TIM_CCMR1_OC2M_FORCE_LOW;
	TIM1_CCMR2 = TIM_CCMR2_OC3M_FORCE_LOW;
	int er = TIM_CCER_CC1NE | TIM_CCER_CC2NE | TIM_CCER_CC3NE;
#endif
#ifdef INVERTED_HIGH
	er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif
	TIM1_CCER = er;
	TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
}

static void delayf(void) {
	TIM6_EGR = TIM_EGR_UG; // Reset arming timeout
	if (!(TIM1_SR & TIM_SR_UIF)) return;
	TIM1_SR = ~TIM_SR_UIF;
	int a = TIM1_CCR1;
	int b = TIM1_CCR3;
	TIM1_CCR1 = b;
	TIM1_CCR3 = a;
}

int playmusic(const char *str, int vol) {
	static const uint16_t arr[] = {15287, 14429, 13619, 12856, 12133, 11452, 10810, 10203, 9630, 9090, 8579, 8097, 7643};
	char *end;
	int tmp = strtol(str, &end, 10); // Tempo
	if (str == end) tmp = 2000; // 120 BPM by default
	else {
		if (tmp < 10 || tmp > 999) return 0; // Sanity check
		tmp = 240000 / tmp;
		str = end;
	}
	if (!vol || ertm || busy) return 0;
	busy = 1;
	resetcom();
#ifdef PWM_ENABLE
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2M_FORCE_LOW;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
	int er = TIM_CCER_CC1E | TIM_CCER_CC1NP | TIM_CCER_CC2NE | TIM_CCER_CC2NP | TIM_CCER_CC3E | TIM_CCER_CC3NP;
#else
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2M_FORCE_HIGH;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
	int er = TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE;
#endif
#ifdef INVERTED_HIGH
	er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif
	TIM1_CCER = er;
	TIM1_PSC = CLK_MHZ / 8 - 1; // 125ns resolution
	for (int a, b, c = 0; (a = *str++);) {
		if (a >= 'a' && a <= 'g') a -= 'c', b = 0; // Low note
		else if (a >= 'A' && a <= 'G') a -= 'C', b = 1; // High note
		else if (a == '_') { // Pause
			TIM1_CCR1 = 0;
			TIM1_CCR3 = 0;
			goto update;
		} else {
			if (a == '+' && !c++) continue; // Octave up
			if (a == '-' && c--) continue; // Octave down
			break; // Invalid specifier
		}
		a = (a + 7) % 7 << 1;
		if (a > 4) --a;
		if (*str == '#') ++a, ++str;
		TIM1_ARR = arr[a] >> (b + c); // Frequency
		TIM1_CCR1 = (DEAD_TIME + CLK_MHZ / 8 - 1) * 8 / CLK_MHZ + vol; // Volume
		TIM1_CCR3 = 0;
	update:
		TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
		a = strtol(str, &end, 10); // Duration
		if (str == end) a = 1;
		else {
			if (a < 1 || a > 99) break; // Sanity check
			str = end;
		}
		delay(tmp * a, delayf);
	}
	TIM1_PSC = 0;
	TIM1_ARR = CLK_KHZ / 24 - 1;
	resetcom();
	busy = 0;
	return !str[-1];
}

void playsound(const char *buf, int vol) { // AU file format, 8-bit linear PCM, mono
	const uint32_t *hdr = (const uint32_t *)buf;
	if (hdr[0] != 0x646e732e || hdr[3] != 0x2000000 || hdr[5] != 0x1000000 || !vol || ertm || busy) return;
	busy = 1;
	resetcom();
#ifdef PWM_ENABLE
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2M_FORCE_LOW;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
	int er = TIM_CCER_CC1E | TIM_CCER_CC1NP | TIM_CCER_CC2NE | TIM_CCER_CC2NP | TIM_CCER_CC3E | TIM_CCER_CC3NP;
#else
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2M_FORCE_HIGH;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
	int er = TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE;
#endif
#ifdef INVERTED_HIGH
	er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif
	TIM1_CCER = er;
	TIM1_CCR1 = 0;
	TIM1_CCR3 = 0;
	TIM1_ARR = CLK_KHZ / 24 - 1;
	TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
	TIM6_PSC = 0;
	TIM6_ARR = CLK_CNT(__builtin_bswap32(hdr[4])) - 1;
	TIM6_EGR = TIM_EGR_UG;
	TIM6_CR1 = TIM_CR1_CEN;
	buf += __builtin_bswap32(hdr[1]);
	for (int len = __builtin_bswap32(hdr[2]);;) {
		if (!(TIM6_SR & TIM_SR_UIF)) continue;
		TIM6_SR = ~TIM_SR_UIF;
		if (len-- <= 0) break;
		int8_t x = *buf++;
		TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_UDIS;
		TIM1_CCR1 = DEAD_TIME + ((x + 128) * vol * CLK_MHZ >> 13);
		TIM1_CCR3 = DEAD_TIME + ((127 - x) * vol * CLK_MHZ >> 13);
		TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
	}
	TIM6_CR1 = 0;
	resetcom();
	busy = 0;
}
