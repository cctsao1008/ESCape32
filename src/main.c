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

#if defined(AM13E)
#include "motor_backend.h"
#include "motor_safety.h"
#include "motor_bemf.h"
#include "irq_vectors.h"
#include "adc_runtime.h"
/* Logical microsecond commutation timebase, not a TI register mapping. */
#define MOTOR_TIME_SHIFT 0
#else
#define MOTOR_TIME_SHIFT IFTIM_XRES
#endif

#define REVISION 17
#define REVPATCH 3

const Cfg cfgdata = {
	.id = 0x32ea,
	.revision = REVISION,
	.revpatch = REVPATCH,
	.name = TARGET_NAME,
	.arm = ARM,                 // Wait for 250ms zero throttle on startup
	.damp = DAMP,               // Complementary PWM (active freewheeling)
	.revdir = REVDIR,           // Reversed motor direction
	.brushed = BRUSHED,         // Brushed mode
	.timing = TIMING,           // Motor timing (15/16 deg) [1..31]
	.sine_range = SINE_RANGE,   // Sine startup range (%) [0 - off, 5..25]
	.sine_power = SINE_POWER,   // Sine startup power (%) [1..15]
	.freq_min = FREQ_MIN,       // Minimum PWM frequency (kHz) [16..48]
	.freq_max = FREQ_MAX,       // Maximum PWM frequency (kHz) [16..96]
	.duty_min = DUTY_MIN,       // Minimum duty cycle (%) [1..100]
	.duty_max = DUTY_MAX,       // Maximum duty cycle (%) [1..100]
	.duty_spup = DUTY_SPUP,     // Maximum duty cycle during spin-up (%) [1..100]
	.duty_ramp = DUTY_RAMP,     // Maximum duty cycle ramp (kERPM) [0..100]
	.duty_rate = DUTY_RATE,     // Duty cycle slew rate (0.1%/ms) [1..100]
	.duty_drag = DUTY_DRAG,     // Drag brake power (%) [0..100]
	.duty_lock = DUTY_LOCK,     // Active drag brake (0 - off, 1 - soft, 2 - hard)
	.throt_mode = THROT_MODE,   // Throttle mode (0 - forward, 1 - forward/reverse, 2 - forward/brake/reverse, 3 - forward/brake)
	.throt_rev = THROT_REV,     // Maximum reverse throttle (0 - 100%, 1 - 75%, 2 - 50%, 3 - 25%)
	.throt_brk = THROT_BRK,     // Maximum brake power (%) [0..100]
	.throt_set = THROT_SET,     // Preset throttle (%) [0..100]
	.throt_ztc = THROT_ZTC,     // Zero-throttle coasting
	.throt_cal = THROT_CAL,     // Automatic throttle calibration
	.throt_min = THROT_MIN,     // Minimum throttle setpoint (us)
	.throt_mid = THROT_MID,     // Middle throttle setpoint (us)
	.throt_max = THROT_MAX,     // Maximum throttle setpoint (us)
	.analog_min = ANALOG_MIN,   // Minimum analog throttle setpoint (mV)
	.analog_max = ANALOG_MAX,   // Maximum analog throttle setpoint (mV)
	.input_mode = INPUT_MODE,   // Input mode (0 - servo/Oneshot125/DSHOT, 1 - analog, 2 - serial, 3 - iBUS, 4 - SBUS/SBUS2, 5 - CRSF, 6 - EXBUS, 7 - HoTT)
	.input_ch1 = INPUT_CH1,     // Throttle channel [0 - off, 1..32]
	.input_ch2 = INPUT_CH2,     // Auxiliary channel [0 - off, 1..32]
	.telem_mode = TELEM_MODE,   // Telemetry mode (0 - KISS, 1 - KISS auto, 2 - iBUS, 3 - S.Port, 4 - CRSF, 5 - MSB, 6 - HoTT)
	.telem_phid = TELEM_PHID,   // Telemetry physical ID [0 - off, 1..2 - iBUS/MSB, 1..4 - SBUS2, 1..8 - CRSF, 1..28 - S.Port]
	.telem_poles = TELEM_POLES, // Number of motor poles for RPM telemetry [2..100]
	.telem_volt = TELEM_VOLT,   // Voltage sensor calibration (~0.6%) [-80..160]
	.telem_curr = TELEM_CURR,   // Current sensor calibration (~0.5%) [-100..200]
	.prot_stall = PROT_STALL,   // Stall protection (ERPM) [0 - off, 1500..3500]
	.prot_temp = PROT_TEMP,     // Temperature threshold (C) [0 - off, 60..140]
	.prot_sens = PROT_SENS,     // Temperature sensor (0 - ESC, 1 - motor, 2 - both)
	.prot_volt = PROT_VOLT,     // Low voltage cutoff per battery cell (V/10) [0 - off, 28..38]
	.prot_cells = PROT_CELLS,   // Number of battery cells [0 - auto, 1..24]
	.prot_curr = PROT_CURR,     // Maximum current (A) [0..999]
	.prot_park = PROT_PARK,     // Parking speed [0..4]
	.music = MUSIC,             // Startup music
	.volume = VOLUME,           // Sound volume (%) [0..100]
	.beacon = BEACON,           // Beacon volume (%) [0..100]
	.bec = BEC,                 // BEC voltage (0 - 5.5V, 1 - 6.5V, 2 - 7.4V, 3 - 8.4V, 4 - 12V)
	.led = LED,                 // LED on/off bits [0..15]
};

__attribute__((__section__(".cfg")))
Cfg cfg = cfgdata;

int throt, brake, ertm, erpm, temp1, temp2, volt, curr, csum, dshotval, beepval = -1;
char analog, telreq, telmode, telphid, flipdir, beacon, dshotext, rearm, auxup;
#if defined(AM13E)
/* ISR-written 16kHz timebase, read by AM13E motor runtime. */
volatile uint32_t tick;
#else
uint32_t tick;
#endif

static int oldstep, step, sine, ival, cutback;
static char prep, sync, fast, lock, ready, reverse;
/* Keep the original LED status state only when a target can use it.
 * The generic AM13E target specifies neither LED_MAP nor LED_STAT.
 */
#if !defined(AM13E) || LED_CNT > 0 || defined(LED_STAT)
static char led;
#endif
static uint32_t tickv;
static volatile char tickf;
#ifndef HALL_MAP
static const int hall;
#else
static int hall;

static int getcode(void) {
	int x = -1;
	for (int i = 0, j = 0; j < 4; ++j) {
		int y = hallcode();
		if (x == y) continue;
		if (++i == 20) hard_fault_handler(); // Unstable signal
		x = y;
		j = 0;
	}
	return x;
}
#endif

/*
6-step commutation sequence:
 #  +|-  COMP  MASK  BEMF
 1  C|B   101   110   101
 2  A|B   011   011   001
 3  A|C   110   101   011
 4  B|C   001   110   010
 5  B|A   111   011   110
 6  C|A   010   101   100
*/

static void nextstep(void) {
	if (sine) { // Sine startup
#if defined(AM13E)
        am13e_app_motor_sine_schedule_us(sine);
#else
		TIM_ARR(IFTIM) = IFTIM_OCR = sine;
		TIM_EGR(IFTIM) = TIM_EGR_UG;
#endif
		if (!prep && step) step = step * 60 - 59; // Switch over from 6-step
		if (reverse) {
			if (--step < 1) step = 360;
		} else {
			if (++step > 360) step = 1;
		}
		int a = step - 1;
		int b = a < 120 ? a + 240 : a - 120;
		int c = a < 240 ? a + 120 : a - 240;
		int p = min(cfg.sine_power << 3, 120 - cutback); // 50% cutback at 15C above prot_temp
#if defined(AM13E)
        am13e_app_motor_sine_write(a, b, c, p, !prep);
#else
		TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_UDIS;
		TIM1_ARR = CLK_KHZ / 24 - 1;
		TIM1_CCR1 = DEAD_TIME + (sinedata[a] * p >> 7);
		TIM1_CCR2 = DEAD_TIME + (sinedata[b] * p >> 7);
		TIM1_CCR3 = DEAD_TIME + (sinedata[c] * p >> 7);
		TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
#endif
#if defined(ERPM_PIN) && !defined(AM13E)
		if (step == 1) GPIO(ERPM_PORT, BSRR) = 1 << (ERPM_PIN + 16);
		else if (step == 181) GPIO(ERPM_PORT, BSRR) = 1 << ERPM_PIN;
#endif
		if (prep) return;
#if defined(AM13E)
        am13e_app_motor_sine_finish();
#else
		TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
		TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
#ifdef PWM_ENABLE
		int er = TIM_CCER_CC1E | TIM_CCER_CC1NP | TIM_CCER_CC2E | TIM_CCER_CC2NP | TIM_CCER_CC3E | TIM_CCER_CC3NP;
#else
		int er = TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E | TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE;
#endif
#ifdef INVERTED_HIGH
		er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif
		TIM1_CCER = er;
		TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
		TIM_DIER(IFTIM) = 0;
#endif
		compctl(0);
		sync = 0;
		prep = 1;
		return;
	}
#ifdef HALL_MAP
	static const char map[][2] = {{2, 4}, {4, 6}, {3, 5}, {6, 2}, {1, 3}, {5, 1}}; // Hall sensor code mapping
	if (hall > 4000) {
		int code = getcode();
		if (code < 1 || code > 6) hard_fault_handler(); // Invalid Hall sensor code
		step = map[code - 1][!!reverse];
	} else
#endif
	if (reverse) {
		if (--step < 1) step = 6;
	} else {
		if (++step > 6) step = 1;
	}
	static const uint16_t seq[] = {0x175, 0xd9, 0x1ab, 0x72, 0x1de, 0xac}; // Commutation sequence
	static int pcc, val, cnt, buf[6];
	int x = seq[step - 1];
	int m = x >> 3; // Energized phase mask
	int p = x & m; // Positive phase
	int n = ~x & m; // Negative phase
	int cc = m >> 3 ^ reverse << 2; // Floating phase
#if defined(AM13E)
    if (cfg.throt_ztc && !throt) p = n = 0;
    am13e_app_motor_sixstep_write(p, n, cc, cfg.damp, reverse);
#else
	int m1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
#ifdef TIM1_CCR5
	int m2 = TIM_CCMR2_OC3PE;
	int er = TIM_CCER_CC5E;
#else
	int m2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_PWM1;
	int er = TIM_CCER_CC4E;
#endif
	if (cfg.throt_ztc && !throt) p = n = 0; // Zero-throttle coasting
	if (p & 1) {
		m1 |= TIM_CCMR1_OC1M_PWM1;
#ifdef PWM_ENABLE
		er |= TIM_CCER_CC1E;
#else
		er |= cfg.damp ? TIM_CCER_CC1E | TIM_CCER_CC1NE : TIM_CCER_CC1E;
#endif
	} else if (n & 1) {
#ifdef PWM_ENABLE
		m1 |= TIM_CCMR1_OC1M_FORCE_LOW;
#else
		m1 |= TIM_CCMR1_OC1M_FORCE_HIGH;
#endif
		er |= TIM_CCER_CC1NE;
	} else {
#ifdef PWM_ENABLE
		m1 |= TIM_CCMR1_OC1M_FORCE_HIGH;
#else
		m1 |= TIM_CCMR1_OC1M_FORCE_LOW;
#endif
		er |= TIM_CCER_CC1NE;
	}
	if (p & 2) {
		m1 |= TIM_CCMR1_OC2M_PWM1;
#ifdef PWM_ENABLE
		er |= TIM_CCER_CC2E;
#else
		er |= cfg.damp ? TIM_CCER_CC2E | TIM_CCER_CC2NE : TIM_CCER_CC2E;
#endif
	} else if (n & 2) {
#ifdef PWM_ENABLE
		m1 |= TIM_CCMR1_OC2M_FORCE_LOW;
#else
		m1 |= TIM_CCMR1_OC2M_FORCE_HIGH;
#endif
		er |= TIM_CCER_CC2NE;
	} else {
#ifdef PWM_ENABLE
		m1 |= TIM_CCMR1_OC2M_FORCE_HIGH;
#else
		m1 |= TIM_CCMR1_OC2M_FORCE_LOW;
#endif
		er |= TIM_CCER_CC2NE;
	}
	if (p & 4) {
		m2 |= TIM_CCMR2_OC3M_PWM1;
#ifdef PWM_ENABLE
		er |= TIM_CCER_CC3E;
#else
		er |= cfg.damp ? TIM_CCER_CC3E | TIM_CCER_CC3NE : TIM_CCER_CC3E;
#endif
	} else if (n & 4) {
#ifdef PWM_ENABLE
		m2 |= TIM_CCMR2_OC3M_FORCE_LOW;
#else
		m2 |= TIM_CCMR2_OC3M_FORCE_HIGH;
#endif
		er |= TIM_CCER_CC3NE;
	} else {
#ifdef PWM_ENABLE
		m2 |= TIM_CCMR2_OC3M_FORCE_HIGH;
#else
		m2 |= TIM_CCMR2_OC3M_FORCE_LOW;
#endif
		er |= TIM_CCER_CC3NE;
	}
#ifdef PWM_ENABLE
	er |= TIM_CCER_CC1NP | TIM_CCER_CC2NP | TIM_CCER_CC3NP;
#endif
#ifdef INVERTED_HIGH
	er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif
	TIM1_CCMR1 = m1;
	TIM1_CCMR2 = m2;
	TIM1_CCER = er;
#endif /* AM13E phase interface */
	compctl(pcc);
	pcc = cc;
	if (ival > 1000 << MOTOR_TIME_SHIFT) {
		val = 1000 << MOTOR_TIME_SHIFT;
		cnt = 0;
	} else if (++cnt == 6) {
		if (abs(val - ival) > ival >> 1) { // Probably desync
			sync = 0;
			fast = 0;
			ival = 5000 << MOTOR_TIME_SHIFT;
			ertm = 100000000;
		}
		val = ival;
		cnt = 0;
	}
#if defined(AM13E)
    am13e_app_motor_bemf_interval_select(ertm);
#else
	if (ertm < 100) { // 600K+ ERPM
#ifdef TIM1_CCR5
		TIM1_CCR5 = 0;
#else
		TIM1_CCR4 = 0;
#endif
		IFTIM_ICMR = IFTIM_ICM3;
		TIM_CR1(IFTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
	} else if (ertm < 200) { // 300K+ ERPM
#ifdef TIM1_CCR5
		TIM1_CCR5 = 0;
#else
		TIM1_CCR4 = 0;
#endif
		IFTIM_ICMR = IFTIM_ICM2;
		TIM_CR1(IFTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
	} else if (ertm < 1000) { // 60K+ ERPM
#ifdef TIM1_CCR5
		TIM1_CCR5 = 0;
#else
		TIM1_CCR4 = 0;
#endif
		IFTIM_ICMR = IFTIM_ICM1;
		TIM_CR1(IFTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
	} else if (ertm < 2000) { // 30K+ ERPM
#ifdef TIM1_CCR5
		TIM1_CCR5 = 0;
#else
		TIM1_CCR4 = 0;
#endif
		IFTIM_ICMR = IFTIM_ICM1;
		TIM_CR1(IFTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS | TIM_CR1_CKD_CK_INT_MUL_2;
	} else { // Minimum PWM frequency
#ifdef TIM1_CCR5
		TIM1_CCR5 = IFTIM_ICFL << 2;
#else
		TIM1_CCR4 = IFTIM_ICFL << 2;
#endif
		IFTIM_ICMR = IFTIM_ICM1;
		TIM_CR1(IFTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS | TIM_CR1_CKD_CK_INT_MUL_4;
	}
	TIM_SR(IFTIM) = 0; // Clear BEMF events before enabling interrupts
	TIM_DIER(IFTIM) = TIM_DIER_UIE | IFTIM_ICIE;
#endif
	buf[step - 1] = hall > 4000 ? hall << MOTOR_TIME_SHIFT : ival;
	if (sync < 6) return;
	ertm = (buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5]) >> (MOTOR_TIME_SHIFT + 1); // Electrical revolution time (us)
#if defined(ERPM_PIN) && !defined(AM13E)
	if (step == 1) GPIO(ERPM_PORT, BSRR) = 1 << (ERPM_PIN + 16);
	else if (step == 4) GPIO(ERPM_PORT, BSRR) = 1 << ERPM_PIN;
#endif
}

static void laststep(void) {
	resetcom();
	if (sine && prep) { // Switch over to 6-step
		step = (step + 29) / 60 + 1;
		if (step > 6) step = 1;
	}
	sine = 0;
	prep = 0;
	if (lock) nextstep();
	else {
#if defined(AM13E)
        am13e_app_motor_drag_brake_write();
#else
#ifdef PWM_ENABLE
		TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM2 | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM2;
		TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM2;
#else
		TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
		TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
#endif
#endif /* AM13E */
	}
#if defined(AM13E)
    am13e_app_motor_commutation_commit();
#else
	TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
#endif
	compctl(0);
	oldstep = step;
	step = 0;
}

/* Shared Rel17 sensorless BEMF control policy.
 *
 * Keep zero-cross filtering, acceleration detection and timing advance
 * in the original Application, not in a second AM13E motor controller.
 * Legacy IFTIM capture values use MOTOR_TIME_SHIFT; the AM13E
 * adapter supplies the same logical units (microseconds, shift = 0).
 *
 * Hardware-specific IRQ acknowledgement, timer scheduling and
 * PWM/commutation output state remain with their respective MCU ports.
 */
static void bemf_timeout_reset(void) {
    sync = 0;
    fast = 0;
    ival = 10000 << MOTOR_TIME_SHIFT;
    ertm = 100000000;
}

/* Return a positive commutation delay for an accepted crossing.
 * Return zero for an early/spurious capture; do not change state.
 */
static int bemf_zero_cross_delay(int capture_ticks) {
    if (capture_ticks < ival >> 1) return 0;
    int u = ival * 3;
    fast = (capture_ticks < u >> 2 || capture_ticks > u >> 1) && ertm < 2000;
    ival = (capture_ticks + u) >> 2;
    return max((ival - (ival * cfg.timing >> 5)) >> 1, 1);
}

#if !defined(AM13E)
void tim1_com_isr(void) {
	if (!(TIM1_DIER & TIM_DIER_COMIE)) return;
#if !defined STM32G4 && !defined AT32F4
	int m1 = TIM1_CCMR1;
	int m2 = TIM1_CCMR2;
#ifdef PWM_ENABLE
	if ((m1 & TIM_CCMR1_OC1M_MASK) == TIM_CCMR1_OC1M_FORCE_HIGH) m1 &= ~TIM_CCMR1_OC1M_MASK;
	if ((m1 & TIM_CCMR1_OC2M_MASK) == TIM_CCMR1_OC2M_FORCE_HIGH) m1 &= ~TIM_CCMR1_OC2M_MASK;
	if ((m2 & TIM_CCMR2_OC3M_MASK) == TIM_CCMR2_OC3M_FORCE_HIGH) m2 &= ~TIM_CCMR2_OC3M_MASK;
#else
	if ((m1 & TIM_CCMR1_OC1M_MASK) == TIM_CCMR1_OC1M_FORCE_LOW) m1 &= ~TIM_CCMR1_OC1M_MASK;
	if ((m1 & TIM_CCMR1_OC2M_MASK) == TIM_CCMR1_OC2M_FORCE_LOW) m1 &= ~TIM_CCMR1_OC2M_MASK;
	if ((m2 & TIM_CCMR2_OC3M_MASK) == TIM_CCMR2_OC3M_FORCE_LOW) m2 &= ~TIM_CCMR2_OC3M_MASK;
#endif
	TIM1_CCMR1 = m1;
	TIM1_CCMR2 = m2;
	TIM1_EGR = TIM_EGR_COMG;
#endif
	TIM1_SR = ~TIM_SR_COMIF;
	nextstep();
}

void iftim_isr(void) { // BEMF zero-crossing
	int er = TIM_DIER(IFTIM);
	int sr = TIM_SR(IFTIM);
	if ((er & TIM_DIER_UIE) && (sr & TIM_SR_UIF)) { // Timeout
		TIM_SR(IFTIM) = ~TIM_SR_UIF;
		TIM_DIER(IFTIM) = 0;
		bemf_timeout_reset();
		return;
	}
	if (!(er & IFTIM_ICIE)) return;
	int t = IFTIM_ICR; // Time since last zero-crossing
	int delay_ticks = bemf_zero_cross_delay(t);
	if (!delay_ticks) return;
	IFTIM_OCR = delay_ticks; // Commutation delay, original IFTIM units
	TIM_EGR(IFTIM) = TIM_EGR_UG;
	TIM_DIER(IFTIM) = 0;
	if (sync < 6) ++sync;
}

#ifdef HALL_MAP
void tim3_isr(void) { // Any change on Hall sensor inputs
	if (TIM3_SR & TIM_SR_UIF) { // Timeout
		TIM3_SR = ~TIM_SR_UIF;
		hall = 0x10000;
		if (sine || !step) return;
		bemf_timeout_reset();
		return;
	}
	hall = (TIM3_CCR1 + hall * 3) >> 2;
	if (hall < 5000 || sine || !step) return;
	ival = hall << MOTOR_TIME_SHIFT;
	TIM1_EGR = TIM_EGR_COMG;
	TIM_EGR(IFTIM) = TIM_EGR_UG;
	TIM_DIER(IFTIM) = 0;
	if (sync < 6) ++sync;
}
#endif /* HALL_MAP legacy IRQ */
#endif /* !AM13E: legacy TIM1 / BEMF / Hall ISRs */

#if defined(AM13E)
/* Physical commutation IRQ must acknowledge the device event first. */
void am13e_app_motor_on_commutation_event(void) {
    nextstep();
}

/* This callback receives *validated* BEMF timing in logical microseconds.
 * The backend owns sampling, comparator selection, filtering and IRQ ack.
 */
int am13e_app_motor_on_bemf_event(int capture_us, int timeout) {
    if (timeout) {
        bemf_timeout_reset();
        return 1;
    }
    int delay_us = bemf_zero_cross_delay(capture_us);
    if (!delay_us) return 0; /* Ignore early capture; keep ECAP1 armed. */
    am13e_app_motor_bemf_commutation_delay_us(delay_us);
    if (sync < 6) ++sync;
    return 1;
}
#endif /* AM13E */

/* Rel17 ADC scaling and protection logic is shared unchanged. */
void adcdata(int t, int u, int v, int c, int a) {
	static int z = 3300, st = -1, su = -1, sa = -1;
	if ((c -= z) >= 0) ready = 1;
	else {
		if (!ready) z += c >> 1;
		c = 0;
	}
	temp1 = max((t = smooth(&st, t, 10)) >> 2, 0); // C
	temp2 = hall || cfg.prot_sens ? max((u = smooth(&su, TEMP_SENS(u), 10)) >> 2, 0) : 0; // C
#if SENS_CNT >= 1
	static int sv = -1;
	volt = smooth(&sv, v * VOLT_MUL * (cfg.telem_volt + 164) >> 14, 7); // V/100
#endif
#if SENS_CNT >= 2
	static int sc = -1, i, q;
	curr = smooth(&sc, c * CURR_MUL * (cfg.telem_curr + 205) >> 11, 4); // A/100
	i += curr; // Current integral
	if (!(tick & 0x3ff0)) {
		csum = (q += i >> 10) * 91 >> 15; // mAh
		i = 0;
	}
#endif
	if (cfg.prot_temp) { // Temperature protection
		int x = -(cfg.prot_temp << 2);
		switch (cfg.prot_sens) {
			case 0:
				x += t;
				break;
			case 1:
				x += u;
				break;
			case 2:
				x += max(t, u);
				break;
		}
		cutback = clamp(x, 0, 60);
	}
	if (!analog) return;
	throt = scale(smooth(&sa, a, 5), cfg.analog_min, cfg.analog_max, cfg.throt_set * 20, 2000);
}

void sys_tick_handler(void) {
#if defined(AM13E)
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    SCB->SCR = 0;
#else
	SCB_ICSR = SCB_ICSR_PENDSVSET; // Continue with low priority
	SCB_SCR = 0; // Resume main loop
#endif
	if (++tick == tickv) tickf = 0;
}

void delay(uint32_t x, void (*f)(void)) {
	__disable_irq();
	tickv = tick + x;
	tickf = 1;
	__enable_irq();
	while (tickf) f();
}

void pend_sv_handler(void) {
	sendtelem();
	if (tick & 0xf) return; // 16kHz -> 1kHz
	adctrig();
#if !defined(AM13E) || LED_CNT > 0
	/* The generic AM13E target has no assigned LED pins (LED_CNT == 0).
	 * No LED GPIO backend is required for an unpopulated option. If an
	 * AM13E board enables LEDs, its real backend must resolve ledctl().
	 */
	static char a = -1;
	int b = cfg.led ? cfg.led : led;
	if (a != b) ledctl(a = b); // Update LED
#endif
}

void hard_fault_handler(void) {
#if defined(AM13E)
    /* Gate-disable must precede cosmetic LED handling on power-stage faults. */
    am13e_app_motor_fault_shutdown();
#if LED_CNT > 0
    /* Optional diagnostic indication, never a substitute for shutdown. */
    ledctl(1);
#endif
    am13e_app_motor_fault_reset();
#else
	ledctl(1); // Indicate error
	TIM1_EGR = TIM_EGR_BG;
	TIM6_PSC = CLK_KHZ / 10 - 1; // 0.1ms resolution
	TIM6_ARR = 9999;
	TIM6_EGR = TIM_EGR_UG;
	TIM6_SR = ~TIM_SR_UIF;
	TIM6_CR1 = TIM_CR1_CEN | TIM_CR1_OPM;
	while (TIM6_CR1 & TIM_CR1_CEN); // Wait for 1s
	WWDG_CR = WWDG_CR_WDGA; // Trigger watchdog reset
#endif
	for (;;); // Never return
}

static void delayf(void) {
#if defined(AM13E)
    /* Legacy TIM6_EGR restarts the 250 ms arming timer; it does NOT
     * service a hardware window watchdog. Use the existing SysTick-based
     * Rel17 arming window while keeping the I/O watchdog independent.
     */
    am13e_app_motor_arming_window_restart();
#else
	TIM6_EGR = TIM_EGR_UG; // Reset arming timeout
#endif
}

static void beep(void) {
	static const char *const beacons[] = {"EG", "FA", "GB", "AB#", "aDGE"};
	static const char *const values[] = {"c6", "C2", "D2C2", "E2D2C2", "F#2E2D2C2", "G#A#G#A#G#2", "G#A#G#A#F#2G#2", "G#A#G#A#E2F#2G#2", "G#A#G#A#D2E2F#2G#2", "G#A#G#A#C2D2E2F#2G#2", 0};
	if (beacon) {
		playmusic(beacons[beacon - 1], cfg.beacon);
		beacon = 0;
	}
	if (beepval < 0) return;
	int i[10], n = 0, x = beepval, vol = max(cfg.volume, 25);
	while (i[n++] = x % 10, x /= 10);
	while (n--) {
		if (x++) delay(8000, delayf);
		playmusic(values[i[n]], vol);
	}
	beepval = -1;
}

#if defined(PARK_PIN) && !defined(AM13E)
static uint8_t park1;
static uint16_t park2, park3;

static int park(void) {
	if (!--park3) return 0;
	if (park1 == 3 || ++park2 < 800) return 1;
	int x = -1;
	for (int i = 0, j = 0; j < 4; ++j) {
		int y = GPIO(PARK_PORT, IDR) & (1 << PARK_PIN);
		if (x == y) continue;
		if (++i == 20) return 0; // Unstable signal
		x = y;
		j = 0;
	}
	switch (park1) {
		case 0:
			if (!x) return 1;
			park1 = 1;
			break;
		case 1:
			if (x) return 1;
			park1 = 2;
			park3 = -1;
			break;
		case 2:
			if (!x) return 1;
			park1 = 3;
			park3 = (0xffff - park3) >> 1;
			reverse = !reverse;
			break;
	}
	park2 = 0;
	return 1;
}
#endif

#if defined(AM13E)
/* TI GCC startup calls extern int main(void); legacy targets retain void. */
int main(void) {
#else
void main(void) {
#endif
	memcpy(_cfg_start, _cfg, _cfg_end - _cfg_start); // Copy configuration to SRAM
	checkcfg();
	const int brushed = cfg.brushed;
	throt = cfg.throt_set * 20;
	brake = cfg.duty_drag;
	lock = cfg.duty_lock;
	telmode = cfg.telem_mode;
	telphid = cfg.telem_phid;
#if defined ANALOG || defined ANALOG_CHAN
	analog = IO_ANALOG;
#endif
	init();
	initgpio();
#if defined(AM13E)
    /* Real ADC0 PA6 NTC/PA28 VBUS raw acquisition, no motor outputs. */
    am13e_app_adc_init();
#endif
#if !defined(AM13E) || LED_CNT > 0
	/* No LED wiring is defined for the generic AM13E target. */
	initled();
#endif
#if !defined(AM13E_PB14_ONLY)
	inittelem();
#endif
#ifndef ANALOG
	initio();
#endif
#if defined(AM13E)
    am13e_app_motor_init();
    am13e_app_motor_bemf_init(); /* CMPSS/ECAP1, no external gate enable */
    /* Readback only; all six PWM pads remain Hi-Z with TBCLK stopped. */
    if (!am13e_app_motor_inactive_aq_boot_preflight()) {
        am13e_app_motor_fault_shutdown();
        am13e_app_motor_fault_reset();
    }
#else
	TIM1_BDTR = TIM_DTG | TIM_BDTR_OSSR | TIM_BDTR_MOE;
	TIM1_ARR = CLK_KHZ / 24 - 1;
	TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
#ifdef STM32G0
	TIM1_CR2 = TIM_CR2_CCPC | TIM_CR2_CCUS | TIM_CR2_MMS_COMPARE_PULSE << 16; // TRGO2=OC1
#else
	TIM1_CR2 = TIM_CR2_CCPC | TIM_CR2_CCUS | TIM_CR2_MMS_COMPARE_PULSE; // TRGO=OC1
#endif
	TIM_PSC(IFTIM) = (CLK_MHZ >> (MOTOR_TIME_SHIFT + 1)) - 1; // 125/250/500ns resolution
	TIM_ARR(IFTIM) = 0;
	TIM_CR1(IFTIM) = TIM_CR1_URS;
	TIM_EGR(IFTIM) = TIM_EGR_UG;
	TIM_CR1(IFTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
#ifdef HALL_MAP
	if (!brushed && getcode() != 7) { // Hybrid mode
		TIM3_CCMR1 = TIM_CCMR1_CC1S_IN_TRC | TIM_CCMR1_IC1F_DTF_DIV_8_N_8;
		TIM3_SMCR = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1F_ED; // Reset on any edge on TI1
		TIM3_CCER = TIM_CCER_CC1E; // IC1 on any edge on TI1
		TIM3_DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;
		TIM3_PSC = CLK_MHZ / 2 - 1; // 500ns resolution
		TIM3_ARR = -1;
		TIM3_CR1 = TIM_CR1_URS;
#ifdef USE_XOR
		TIM3_CR2 = TIM_CR2_TI1S;
#endif
		TIM3_EGR = TIM_EGR_UG;
		TIM3_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
		hall = 0x10000;
	}
#endif
#endif /* !AM13E initial TIM1 / BEMF / Hall setup */
#if defined(AM13E)
    am13e_app_motor_runtime_tick_init();
    /* Boot hands off with PRIMASK set. The board backend must first
     * prove safe bridge outputs, vectors and interrupt priorities. */
    am13e_app_motor_runtime_enable_interrupts();
#else
	nvic_set_priority(NVIC_PENDSV_IRQ, 0x80);
	STK_RVR = CLK_KHZ / 16 - 1; // 16kHz
	STK_CVR = 0;
	STK_CSR = STK_CSR_ENABLE | STK_CSR_TICKINT | STK_CSR_CLKSOURCE_AHB;
#endif
#ifndef ANALOG
#if SENS_CNT >= 1
	int cells = cfg.prot_cells;
	while (!ready) __WFI(); // Wait for sensors
	if (!cells) cells = (volt + 439) / 440; // Assume maximum 4.4V per battery cell
#endif
#if defined(AM13E)
    int reset_flags = am13e_app_motor_reset_flags();
    int watchdog_poweron = !!(reset_flags & AM13E_APP_RESET_WATCHDOG);
    int watchdog_arm = !!(reset_flags & AM13E_APP_RESET_FORCE_ARM);
#else
	int csr = RCC_CSR;
	RCC_CSR = RCC_CSR_RMVF; // Clear reset flags
    int watchdog_poweron = !!(csr & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF));
    int watchdog_arm = !!(csr & RCC_CSR_WWDGRSTF);
#endif
	if (!watchdog_poweron) { // Power-on
		const char *str = cfg.music;
		if (str[0] == '~') playsound(_eod, clamp(atoi(str + 1), 0, 100));
		else playmusic(str, cfg.volume);
#if SENS_CNT >= 1
		if (cfg.prot_volt) { // Report the number of battery cells
			beepval = cells;
			delay(4000, delayf);
			beep();
		}
#endif
	}
	if (cfg.arm || watchdog_arm) { // Arming required
	rearm:
#if defined(AM13E)
        am13e_app_motor_arming_window_start();
        throt = 1;
        while (!am13e_app_motor_arming_window_expired()) {
            __WFI();
            beep();
            if (throt) am13e_app_motor_arming_window_restart();
        }
        throt = 0;
        rearm = 0;
        am13e_app_motor_arming_window_stop();
#else
		TIM6_PSC = CLK_KHZ / 10 - 1; // 0.1ms resolution
		TIM6_ARR = 2499; // 250ms
		TIM6_CR1 = TIM_CR1_URS;
		TIM6_EGR = TIM_EGR_UG;
		TIM6_CR1 = TIM_CR1_CEN | TIM_CR1_URS;
		TIM6_SR = ~TIM_SR_UIF;
		throt = 1;
		while (!(TIM6_SR & TIM_SR_UIF)) { // Wait for 250ms zero throttle
			__WFI();
			beep();
			if (!throt) continue;
			TIM6_EGR = TIM_EGR_UG;
		}
		throt = 0;
		rearm = 0;
		TIM6_CR1 = 0;
#endif
		playmusic(hall ? "G_GC" : "GC", cfg.volume);
	}
#endif
	laststep();
#if SENS_CNT >= 1
	int cutoff = 0;
#endif
	PID bpid = {.Kp = 50, .Ki = 0, .Kd = 1000}; // Stall protection
#if SENS_CNT >= 2
	PID cpid = {.Kp = 80, .Ki = 0, .Kd = 600}; // Overcurrent protection
#endif
	for (int curduty = 0, running = 0, braking = 2, boost = 0, choke = 0, n = 0;;) {
#if !defined(AM13E)
		int ccr, arr = CLK_KHZ / cfg.freq_min;
#endif
		int input = rearm ? 0 : throt;
		int range = cfg.sine_range * 20;
		int delta = range ? 10 : 0;
		int newduty = 0;
		if (!running) curduty = 0;
		if (!input) { // Neutral
			if (braking == 1) braking = 2; // Reverse after braking
			if (lock != cfg.duty_lock && !brushed) { // Switch brake mode
				lock = cfg.duty_lock;
				if (!running) laststep();
			}
			if (sync < 6 || erpm < 800 || lock == 2) { // Drag brake
#if defined(PARK_PIN) && !defined(AM13E)
				if (cfg.prot_park && running && park()) { // Parking
					sine = (1000 << MOTOR_TIME_SHIFT) / cfg.prot_park;
					ertm = 100000000;
					erpm = 0;
					goto skipduty;
				}
#endif
				curduty = lock ? min(brake, 100 - cutback) : brake * 20; // 60% cutback at 15C above prot_temp
				running = 0;
				goto setduty;
			}
			boost = 0; // Coasting
			goto calcduty;
		}
#if defined(PARK_PIN) && !defined(AM13E)
		park1 = 0;
		park2 = 0;
		park3 = -1;
#endif
		if (input < 0) { // Reverse
			if ((cfg.throt_mode == 2 && braking != 2) || cfg.throt_mode == 3) { // Proportional brake
				curduty = scale(input, -2000, 0, cfg.throt_brk * 20, brake * 20);
				running = 0;
				braking = 1;
				goto setduty;
			}
			input = input * (cfg.throt_rev - 4) >> 2;
			reverse = !cfg.revdir ^ flipdir;
			running = 1;
		} else { // Forward
			reverse = cfg.revdir ^ flipdir;
			running = 1;
			braking = 0;
		}
		if (range + (sine ? delta : -delta) < input) newduty = scale(input, range + delta, 2000, cfg.duty_min * 20, cfg.duty_max * 20);
		else sine = scale(input, 0, range - delta, 1000 << MOTOR_TIME_SHIFT, cfg.prot_stall ? (333333 << MOTOR_TIME_SHIFT) / cfg.prot_stall : 145 << MOTOR_TIME_SHIFT);
		if (sine) { // Sine startup
			if (!newduty) {
				if (!ertm) goto skipduty;
				ertm = sine * (180 >> MOTOR_TIME_SHIFT);
				erpm = 60000000 / ertm;
				goto skipduty;
			}
			__disable_irq();
			if (prep) { // Switch over to 6-step
				int a = step - 1;
				int b = a / 60;
				int c = b * 60;
                int delay_ticks = sine * (reverse ? (void)(++b == 6 && (b = 0)), a - c + 1 : c - a + 60); // Commutation delay
#if defined(AM13E)
                am13e_app_motor_bemf_sine_exit_us(delay_ticks);
#else
				IFTIM_OCR = delay_ticks;
				TIM_ARR(IFTIM) = (1 << (MOTOR_TIME_SHIFT + 16)) - 1;
				TIM_EGR(IFTIM) = TIM_EGR_UG;
#endif
				step = b + 1;
			}
			sine = 0;
			prep = 0;
			sync = 0;
			fast = 0;
			ival = 10000 << MOTOR_TIME_SHIFT;
			nextstep();
			__enable_irq();
			initpid(&bpid, 10000 << MOTOR_TIME_SHIFT);
			curduty = 0;
			boost = 0;
		}
	calcduty:
		if (brushed && step != reverse + 1) step = 0; // Change brushed direction
		if ((newduty += boost - choke) < 0) newduty = 0;
		if (ertm) { // Variable PWM frequency
#if !defined(AM13E)
			arr = scale(ertm, 1000, 2000, CLK_KHZ / cfg.freq_max, arr); // 30..60 kERPM
#endif
			erpm = 60000000 / ertm;
		}
		int maxduty = min(scale(erpm, 0, cfg.duty_ramp * 1000, cfg.duty_spup * 20, 2000), 2000 - cutback * 25); // 75% cutback at 15C above prot_temp
		if (newduty > maxduty) newduty = maxduty;
		int a = fast ? 0 : cfg.duty_rate;
		int b = a >> 3;
		if (n < (a & 7)) ++b;
		if (++n == 8) n = 0;
		if (curduty > newduty ? sync < 6 || (curduty -= b) < newduty : (curduty += b) > newduty) curduty = newduty; // Duty cycle slew rate limiting
	setduty:
#if defined(AM13E)
        /* Backend maps Rel17 logical duty and frequency policy onto MCPWM.
         * This interface has no assumed physical frequency/dead-time.
         */
        am13e_app_motor_pwm_apply(curduty, cfg.freq_min, cfg.freq_max,
                                   ertm, cfg.damp, lock, brushed, running);
#else
#ifdef FULL_DUTY // Allow 100% duty cycle
		ccr = scale(curduty, 0, 2000, lock || (running && cfg.damp) ? DEAD_TIME : 0, arr--);
#else
		ccr = scale(curduty, 0, 2000, lock || (running && cfg.damp) ? DEAD_TIME : 0, brushed ? arr - (CLK_MHZ * 3 >> 1) : arr);
#endif
		TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_UDIS;
		TIM1_ARR = arr;
		TIM1_CCR1 = ccr;
		TIM1_CCR2 = ccr;
		TIM1_CCR3 = ccr;
		TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
#endif
	skipduty:
		if (running && !step) { // Start motor
			if (brushed) {
#if defined(AM13E)
                am13e_app_motor_brushed_write(reverse, cfg.damp);
                /* Rel17 starts brushed PWM at its COM update. Route the
                 * equivalent board-controlled enable through the same
                 * MCPWM lifecycle contract as six-step, not a separate path.
                 * Physical output remains blocked until gate/trip wiring
                 * and dead-band are established in that backend.
                 */
                am13e_app_motor_commutation_enable(1);
#else
				int m1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
				int m2 = TIM_CCMR2_OC3PE;
#ifdef PWM_ENABLE
				int er = TIM_CCER_CC1E | TIM_CCER_CC1NP | TIM_CCER_CC2E | TIM_CCER_CC2NP | TIM_CCER_CC3E | TIM_CCER_CC3NP;
				if (reverse) {
					m1 |= TIM_CCMR1_OC1M_FORCE_LOW | TIM_CCMR1_OC2M_PWM1;
					m2 |= TIM_CCMR2_OC3M_FORCE_LOW;
				} else {
					m1 |= TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2M_FORCE_LOW;
					m2 |= TIM_CCMR2_OC3M_PWM1;
				}
#else
				int er = 0;
				if (reverse) {
					m1 |= TIM_CCMR1_OC1M_FORCE_HIGH | TIM_CCMR1_OC2M_PWM1;
					m2 |= TIM_CCMR2_OC3M_FORCE_HIGH;
					er |= TIM_CCER_CC1NE | TIM_CCER_CC2E | TIM_CCER_CC3NE;
					if (cfg.damp) er |= TIM_CCER_CC2NE;
				} else {
					m1 |= TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2M_FORCE_HIGH;
					m2 |= TIM_CCMR2_OC3M_PWM1;
					er |= TIM_CCER_CC1E | TIM_CCER_CC2NE | TIM_CCER_CC3E;
					if (cfg.damp) er |= TIM_CCER_CC1NE | TIM_CCER_CC3NE;
				}
#endif
#ifdef INVERTED_HIGH
				er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif
				TIM1_CCMR1 = m1;
				TIM1_CCMR2 = m2;
				TIM1_CCER = er;
				TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
#endif
				step = reverse + 1;
				ertm = 600; // 100K ERPM (freq_min/duty_spup/duty_ramp have no effect)
				goto tick;
			}
			__disable_irq();
			step = oldstep;
			ival = 10000 << MOTOR_TIME_SHIFT;
			ertm = 100000000;
			nextstep();
#if defined(AM13E)
            am13e_app_motor_commutation_commit();
            am13e_app_motor_commutation_enable(1);
            am13e_app_motor_bemf_sine_exit_us(0xffff);
#else
			TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
#ifdef SW_BLANKING
			TIM1_DIER |= TIM_DIER_COMIE | TIM_DIER_UIE | TIM_DIER_CC4IE;
#else
			TIM1_DIER |= TIM_DIER_COMIE;
#endif
			TIM_ARR(IFTIM) = IFTIM_OCR = (1 << (MOTOR_TIME_SHIFT + 16)) - 1;
			TIM_EGR(IFTIM) = TIM_EGR_UG;
#endif
			__enable_irq();
			initpid(&bpid, 10000 << MOTOR_TIME_SHIFT);
			boost = 0;
		} else if (!running && step) { // Stop motor
			__disable_irq();
#if defined(AM13E)
            am13e_app_motor_commutation_enable(0);
            am13e_app_motor_bemf_stop();
#else
#ifdef SW_BLANKING
			TIM1_DIER &= ~(TIM_DIER_COMIE | TIM_DIER_UIE | TIM_DIER_CC4IE);
#else
			TIM1_DIER &= ~TIM_DIER_COMIE;
#endif
			TIM_DIER(IFTIM) = 0;
			TIM_ARR(IFTIM) = 0;
			TIM_EGR(IFTIM) = TIM_EGR_UG;
#endif
			laststep();
			sync = 0;
			fast = 0;
			ertm = 0;
			erpm = 0;
			__enable_irq();
		}
	tick:
#if defined(AM13E)
        SCB->SCR = SCB_SCR_SLEEPONEXIT_Msk;
#else
		SCB_SCR = SCB_SCR_SLEEPONEXIT; // Suspend main loop
#endif
		__WFI();
		if (tick & 0xf) continue; // 16kHz -> 1kHz
#ifndef ANALOG
		if (rearm && !running) goto rearm;
		if (++auxup == 100) { // Restore brake after 100ms timeout
			brake = cfg.duty_drag;
			auxup = 0;
		}
#if SENS_CNT >= 1
		if (cutoff < 3000) cutoff = volt < cfg.prot_volt * cells * 10 ? cutoff + 1 : 0;
		else rearm = 1; // Low voltage cutoff after 3s
#endif
#endif
		boost = cfg.prot_stall ? clamp(boost + (calcpid(&bpid, hall > 4000 ? hall : ival >> MOTOR_TIME_SHIFT, 20000000 / cfg.prot_stall - 800) >> 16), 0, 160) : 0; // Up to 8%
#if SENS_CNT >= 2
		choke = cfg.prot_curr ? clamp(choke + (calcpid(&cpid, curr, cfg.prot_curr * 100) >> 10), 0, 2000) : 0;
#endif
		beep();
#ifdef LED_STAT
		int x = 0;
		if (running) {
			if ((tick << (throt < 0) & 0x1fff) < (sine ? 0x1800 : 0x1000)) x = LED_CNT >= 2 ? 2 : 1;
		} else if (curduty) {
			if ((tick & (lock ? 0x2fff : 0x3fff)) < 0x400) x = LED_CNT >= 3 ? 4 : LED_CNT;
		}
		if (cutback || cutoff || choke) x |= 1;
		led = x;
#endif
	}
}
