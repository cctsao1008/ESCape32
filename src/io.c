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
#include "bidir_codec.h"
static inline __attribute__((always_inline)) int exbus_retry_step(uint8_t *count) {
    if(!*count)return 0;
    return --*count&1U ? 1 : 2;
}
static inline __attribute__((always_inline)) int cli_high_timeout(uint8_t *count) { return ++*count>=16U; }
#include "exbus_autobaud.h"

#if defined(AM13E)
#include "io_backend.h"
#include "bidir_codec.h"
#include "command_reply.h" /* DShot save waits for pending BiDShot reply */
#include "service_reset.h"
#else
#ifdef AT32F4
#define USART2_TDR USART2_DR
#define USART2_RDR USART2_DR
#define USART2_ISR USART2_SR
#define USART_CR1_M0 USART_CR1_M
#define USART_ISR_FE USART_SR_FE
#define USART_ISR_NF USART_SR_NE
#endif
#endif /* !AM13E: STM32 / AT32 USART register names */

#if !defined(AM13E)
static void entryirq(void);
static void calibirq(void);
static void servoirq(void);
static void dshotirq(void);
static void cliirq(void);
#ifdef IO_PA2
static void serialirq(void);
static int serialfunc(int len);
static int ibusfunc(int len);
static int sbusfunc(int len);
static void sbuspacket(unsigned n);
static int crsffunc(int len);
static int exbusfunc(int len);
static int hottfunc(int len);
static int (*iofunc)(int len);
#endif
static void (*ioirq)(void);
static char dshotinv, iobuf[1024] __attribute__((aligned(2)));
static uint16_t dshotarr1, dshotarr2;
#endif /* !AM13E: legacy timer and capture state */

#if defined(AM13E)
static char iobuf[1024] __attribute__((aligned(2)));
static unsigned io_valid_frames, sbus2_reply_remaining, sbus2_reply_index;
static uint32_t io_exbus_device_id;
#if defined(AM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED) || defined(REL17_CALIBRATION_TEST_ADAPTER)
static struct { uint8_t count; int16_t error,high,low; } servo_calibration_state;
static unsigned servo_capture_period;
static int servo_calibration_finished;
static void calibirq(void);
#endif
static int serialfunc(int len), ibusfunc(int len), sbusfunc(int len);
static int crsffunc(int len), exbusfunc(int len), hottfunc(int len), exbusresp(int id);
static void sbuspacket(unsigned n);
static inline __attribute__((always_inline)) unsigned crsfresponse(const char *buf, char *res);
static void io_feed_watchdog(void) {
    ++io_valid_frames;
    am13e_app_io_watchdog_feed();
}
#ifdef REL17_IO_TEST_OBSERVER
static int io_raw_throttle=-1, io_raw_brake=-1;
static bool io_raw_channels;
#endif
static int io_core_frame(const uint8_t *frame,unsigned length,int (*parse)(int)) {
    if(!frame || length>sizeof iobuf)return 0;
    memcpy(iobuf,frame,length);
    const unsigned before=io_valid_frames;
    (void)parse((int)length);
    return io_valid_frames!=before;
}
#endif

/* Rel17 input PWM throttle conversion is shared. */
static void setthrot(int x) {
#if defined(AM13E) && defined(REL17_IO_TEST_OBSERVER)
    io_raw_throttle=x;io_raw_channels=true;
#endif
	if (x < 0) return;
	throt = cfg.throt_mode ?
		x < cfg.throt_mid - 50 ? scale(x, cfg.throt_min, cfg.throt_mid - 50, -2000, 0):
		x > cfg.throt_mid + 50 ? scale(x, cfg.throt_mid + 50, cfg.throt_max, 0, 2000): 0:
		x > cfg.throt_min + 50 ? scale(x, cfg.throt_min + 50, cfg.throt_max, 0, 2000): 0;
}

#if defined IO_PA2 || defined IO_AUX || defined(AM13E)
static void setbrake(int x) {
#if defined(AM13E) && defined(REL17_IO_TEST_OBSERVER)
    io_raw_brake=x;io_raw_channels=true;
#endif
	if (x < 0) return;
	brake = scale(x, 1100, 1900, 0, cfg.duty_drag);
	auxup = 0;
}
#endif

#if defined(IO_AUX) && !defined(AM13E)
void iotim2_isr(void) {
#if IOTIM2 == TIM16 || IOTIM2 == TIM17
	static uint16_t t1;
	uint16_t t2 = TIM_CCR1(IOTIM2);
	uint16_t er = TIM_CCER(IOTIM2);
	TIM_CCER(IOTIM2) = er ^ TIM_CCER_CC1P;
	if (!(er & TIM_CCER_CC1P)) { // Rising edge
		t1 = t2;
		return;
	}
	int x = t2 - t1;
#else
	int x = TIM_CCR2(IOTIM2);
#endif
	setbrake(x);
}
#endif

#if !defined(AM13E)
/* All physical STM32 timer/DMA/IRQ state transitions remain unchanged.
 * AM13E initio() is supplied by its platform backend, not a no-op.
 */
void initio(void) {
	ioirq = entryirq;
	TIM_BDTR(IOTIM) = TIM_BDTR_MOE;
	TIM_CCMR1(IOTIM) = TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_CK_INT_N_8;
	TIM_SMCR(IOTIM) = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1FP1; // Reset on rising edge on TI1
	TIM_CCER(IOTIM) = TIM_CCER_CC1E; // IC1 on rising edge on TI1
	TIM_DIER(IOTIM) = TIM_DIER_UIE | TIM_DIER_CC1IE;
	TIM_PSC(IOTIM) = CLK_MHZ - 1; // 1us resolution
	TIM_ARR(IOTIM) = -1;
	TIM_CR1(IOTIM) = TIM_CR1_URS;
	TIM_EGR(IOTIM) = TIM_EGR_UG;
	TIM_CR1(IOTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
#ifdef IO_AUX
#if IOTIM2 == TIM16 || IOTIM2 == TIM17
	TIM_CCMR1(IOTIM2) = TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_DTF_DIV_8_N_8;
	TIM_CCER(IOTIM2) = TIM_CCER_CC1E; // IC1 on rising edge on TI1
	TIM_DIER(IOTIM2) = TIM_DIER_CC1IE;
#else
	TIM_CCMR1(IOTIM2) = TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_DTF_DIV_8_N_8 | TIM_CCMR1_CC2S_IN_TI1 | TIM_CCMR1_IC2F_DTF_DIV_8_N_8;
	TIM_SMCR(IOTIM2) = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1FP1; // Reset on rising edge on TI1
	TIM_CCER(IOTIM2) = TIM_CCER_CC2E | TIM_CCER_CC2P; // IC2 on falling edge on TI1
	TIM_DIER(IOTIM2) = TIM_DIER_CC2IE;
#endif
	TIM_PSC(IOTIM2) = CLK_MHZ - 1; // 1us resolution
	TIM_ARR(IOTIM2) = -1;
	TIM_EGR(IOTIM2) = TIM_EGR_UG;
	TIM_CR1(IOTIM2) = TIM_CR1_CEN;
#endif
}

static void entryirq(void) {
	static char n, d;
    static uint8_t c;
	if (TIM_SR(IOTIM) & TIM_SR_UIF) { // Timeout ~66ms
		TIM_SR(IOTIM) = ~TIM_SR_UIF;
		if (!IOTIM_IDR) { // Low level
			if (IO_ANALOG) goto analog;
			n = 0;
			return;
		}
		if (!cli_high_timeout(&c)) return; // Wait for ~1s before entering CLI
		ioirq = cliirq;
#ifdef IO_PA2
		io_serial();
#ifdef IO_RXTX
		GPIOA_PUPDR |= 0x80000000; // A15 (pull-down)
		GPIOA_MODER &= ~0x40000000; // A15 (USART2_RX)
		TIM15_ARR = CLK_CNT(20000) - 1;
		TIM15_EGR = TIM_EGR_UG;
		TIM15_SR = ~TIM_SR_UIF;
		TIM15_CR1 = TIM_CR1_CEN | TIM_CR1_OPM;
		while (TIM15_CR1 & TIM_CR1_CEN) { // Wait for 50us high level on A15
			if (!(GPIOA_IDR & 0x8000)) { // A15 low
				USART2_CR3 = USART_CR3_HDSEL;
				break;
			}
		}
#else
		USART2_CR3 = USART_CR3_HDSEL;
#endif
		USART2_BRR = CLK_CNT(38400);
		USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
#else
		TIM3_CCER = 0;
		TIM3_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM2 | TIM_CCMR1_CC2S_IN_TI1 | TIM_CCMR1_IC2F_CK_INT_N_8;
		TIM3_SMCR = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1F_ED; // Reset on any edge on TI1
		TIM3_CCER = TIM_CCER_CC2E | TIM_CCER_CC2P; // IC2 on falling edge on TI1
		TIM3_SR = ~TIM_SR_CC2IF;
		TIM3_DIER = TIM_DIER_CC2IE;
		TIM3_PSC = 0;
		TIM3_ARR = CLK_CNT(38400) - 1; // Bit time
		TIM3_CCR1 = CLK_CNT(76800); // Half-bit time
		TIM3_EGR = TIM_EGR_UG;
		TIM3_CR1 = TIM_CR1_CEN;
#endif
		return;
	}
	int t = TIM_CCR1(IOTIM); // Time between two rising edges
	if (IO_ANALOG) {
	analog:
#ifndef ANALOG_CHAN
		io_analog();
		analog = 1;
#endif
		return;
	}
	if (!n++) return; // First capture is always invalid
	IWDG_KR = IWDG_KR_START;
#ifdef IO_PA2
	if (cfg.input_mode >= 2) {
		ioirq = serialirq;
		io_serial();
		switch (cfg.input_mode) {
			case 2: // Serial
				iofunc = serialfunc;
				USART2_BRR = CLK_CNT(SERIAL_BR);
				break;
			case 3: // iBUS
				iofunc = ibusfunc;
				USART2_BRR = CLK_CNT(115200);
				break;
			case 4: // SBUS/SBUS2
				iofunc = sbusfunc;
				USART2_BRR = CLK_CNT(100000);
				USART2_CR1 = USART_CR1_PCE | USART_CR1_M0;
				USART2_CR2 = USART_CR2_STOPBITS_2;
#ifndef AT32F4
				USART2_CR2 |= USART_CR2_RXINV | USART_CR2_TXINV;
				GPIOA_PUPDR = (GPIOA_PUPDR & ~0x30) | 0x20; // A2 (pull-down)
#endif
				TIM15_PSC = CLK_MHZ / 8 - 1; // 125ns resolution
				TIM15_ARR = -1;
				TIM15_EGR = TIM_EGR_UG;
				TIM15_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
				break;
			case 5: // CRSF
				iofunc = crsffunc;
				USART2_BRR = CLK_CNT(416666);
				break;
#ifndef DISABLE_EXBUS
			case 6: // EXBUS
				iofunc = exbusfunc;
				USART2_BRR = CLK_CNT(125000);
				break;
#endif
#ifndef DISABLE_HOTT
			case 7: // HoTT
				iofunc = hottfunc;
				USART2_BRR = CLK_CNT(115200);
				break;
#endif
		}
		USART2_CR3 = USART_CR3_HDSEL | USART_CR3_DMAT | USART_CR3_DMAR;
		USART2_CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
		DMA1_CPAR(USART2_RX_DMA) = (uint32_t)&USART2_RDR;
		DMA1_CMAR(USART2_RX_DMA) = (uint32_t)iobuf;
		DMA1_CPAR(USART2_TX_DMA) = (uint32_t)&USART2_TDR;
		DMA1_CMAR(USART2_TX_DMA) = (uint32_t)iobuf;
		DMA1_CNDTR(USART2_RX_DMA) = sizeof iobuf;
		return;
	}
#endif
	if (TIM_PSC(IOTIM)) {
		if (t > 2000) { // Servo/Oneshot125
			if (n < 8) return;
			ioirq = calibirq;
			TIM_DIER(IOTIM) = TIM_DIER_CC1IE;
			TIM_CR1(IOTIM) = TIM_CR1_CEN;
			calibirq();
			return;
		}
		TIM_PSC(IOTIM) = TIM_PSC(IOTIM) == CLK_MHZ - 1 ? CLK_MHZ / 8 - 1 : 0;
		TIM_EGR(IOTIM) = TIM_EGR_UG;
		n = 0;
		return;
	}
	int m = 2;
	while (t >= CLK_CNT(800000)) t >>= 1, --m;
	if (d != m) {
		d = m;
		n = 1;
		return;
	}
	if (m < 0 || n < 4) return;
	ioirq = dshotirq;
	dshotarr1 = CLK_CNT(150000 << m) - 1;
	dshotarr2 = CLK_CNT(375000 << m) - 1;
	TIM_CCER(IOTIM) = 0;
	TIM_CCMR1(IOTIM) = TIM_CCMR1_CC1S_IN_TRC | TIM_CCMR1_IC1F_CK_INT_N_8;
	TIM_SMCR(IOTIM) = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1F_ED; // Reset on any edge on TI1
	TIM_CCER(IOTIM) = TIM_CCER_CC1E; // IC1 on any edge on TI1
	TIM_DIER(IOTIM) = TIM_DIER_UIE;
	TIM_ARR(IOTIM) = dshotarr1; // Frame reset timeout
	TIM_EGR(IOTIM) = TIM_EGR_UG;
	DMA1_CPAR(IOTIM_DMA) = (uint32_t)&TIM_CCR1(IOTIM);
	DMA1_CMAR(IOTIM_DMA) = (uint32_t)iobuf;
}



static void servoirq(void) {
	int x = TIM_CCR2(IOTIM);
	if (x >= 28 && x <= 32) { // Telemetry request
		IWDG_KR = IWDG_KR_RESET;
		telreq = 1;
		return;
	}
	if (x < 800 || x > 2200) { // Invalid signal
		if (ertm) return;
		WWDG_CR = WWDG_CR_WDGA;
	}
	IWDG_KR = IWDG_KR_RESET;
	setthrot(x);
}

static void dshotirq(void) {
	static char n;
	if (!(TIM_DIER(IOTIM) & TIM_DIER_UIE) || !(TIM_SR(IOTIM) & TIM_SR_UIF)) return;
	TIM_SR(IOTIM) = ~TIM_SR_UIF;
	if ((TIM_DIER(IOTIM) & TIM_DIER_CC1DE) && DMA1_CNDTR(IOTIM_DMA) == 32 && dshotinv == IOTIM_IDR) {
		if (++n < 4) return;
		TIM_ARR(IOTIM) = -1; // Don't trigger UEV for timebase consistency
		TIM_CR1(IOTIM) = TIM_CR1_CEN | TIM_CR1_ARPE;
		TIM_DIER(IOTIM) = TIM_DIER_CC1DE;
		return;
	}
	DMA1_CCR(IOTIM_DMA) = 0;
	DMA1_CNDTR(IOTIM_DMA) = 32;
	DMA1_CCR(IOTIM_DMA) = DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_CIRC | DMA_CCR_MINC | DMA_CCR_PSIZE_16BIT | DMA_CCR_MSIZE_16BIT;
	TIM_DIER(IOTIM) = TIM_DIER_UIE | TIM_DIER_CC1DE;
	dshotinv = IOTIM_IDR;
	n = 0;
}

static void dshotreset(void) {
#ifdef AT32F4 // Errata 1.5.1
	RCC_APB2RSTR = RCC_APB2RSTR_TIM15RST;
	RCC_APB2RSTR = 0;
	TIM15_BDTR = TIM_BDTR_MOE;
	TIM15_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
#else
	TIM_CCER(IOTIM) = 0;
	TIM_DIER(IOTIM) = 0;
	TIM_CR2(IOTIM) = 0;
#endif
	DMA1_CCR(IOTIM_DMA) = 0;
	DMA1_CMAR(IOTIM_DMA) = (uint32_t)iobuf;
	DMA1_CNDTR(IOTIM_DMA) = 32;
	DMA1_CCR(IOTIM_DMA) = DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_CIRC | DMA_CCR_MINC | DMA_CCR_PSIZE_16BIT | DMA_CCR_MSIZE_16BIT;
	TIM_ARR(IOTIM) = -1;
	TIM_EGR(IOTIM) = TIM_EGR_UG;
	TIM_CCMR1(IOTIM) = TIM_CCMR1_CC1S_IN_TRC | TIM_CCMR1_IC1F_CK_INT_N_8;
	TIM_SMCR(IOTIM) = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1F_ED; // Reset on any edge on TI1
	TIM_CCER(IOTIM) = TIM_CCER_CC1E; // IC1 on any edge on TI1
	TIM_DIER(IOTIM) = TIM_DIER_CC1DE;
}

static void dshotresync(void) {
	if (dshotinv) dshotreset();
	TIM_CR1(IOTIM) = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_URS;
	TIM_ARR(IOTIM) = dshotarr1; // Frame reset timeout
	TIM_EGR(IOTIM) = TIM_EGR_UG;
	TIM_SR(IOTIM) = ~TIM_SR_UIF;
	TIM_DIER(IOTIM) = TIM_DIER_UIE;
}

#endif /* !AM13E: legacy capture, calibration and DSHOT timer control */

/* DSHOT CRC and command semantics are platform-independent. */
static int dshotcrc(int x, int inv) {
	int a = x;
	for (int b = x; b >>= 4; a ^= b);
	if (inv) a = ~a;
	return a & 0xf;
}

/* Rel17 DSHOT command, throttle and settings state machine.
 * Called after physical framing and CRC acceptance on either MCU family.
 */
static char cmd, cnt, rep;
#if defined(AM13E)
/* DSHOT_SAVE_SETTINGS arrives in ECAP0 IRQ at highest motor priority.
 * Never erase/program Bank0 Flash from the DShot RX interrupt. PendSV
 * services the request after TX completes, with a stable motor stop.
 */
/* 1 = Save, 2 = Reset. Both source commands may write configuration. */
static volatile uint8_t dshot_save_pending;
static volatile uint8_t dshot_save_running;
#endif

static inline __attribute__((always_inline)) void dshot_apply_packet(int x) {
	int tlm = x & 0x10;
	x >>= 5;
	if (!x || x > 47) {
		if (tlm) telreq = 1; // Telemetry request
		throt = x ? cfg.throt_mode ? (x > 1047 ? x - 1047 : 47 - x) << 1 : x - 47 : 0;
		cmd = 0;
		return;
	}
	if (!tlm || ertm) return; // Telemetry bit must be set, motor must be stopped
	if (cmd != x) {
		cmd = x;
		cnt = 0;
	}
	if (cnt < 10) ++cnt;
	switch (cmd) {
		case 1: // DSHOT_CMD_BEACON1
		case 2: // DSHOT_CMD_BEACON2
		case 3: // DSHOT_CMD_BEACON3
		case 4: // DSHOT_CMD_BEACON4
		case 5: // DSHOT_CMD_BEACON5
			beacon = cmd;
			break;
		case 7: // DSHOT_CMD_SPIN_DIRECTION_1
			if (cnt != 6) break;
			cfg.revdir = 0;
			break;
		case 8: // DSHOT_CMD_SPIN_DIRECTION_2
			if (cnt != 6) break;
			cfg.revdir = 1;
			break;
		case 9: // DSHOT_CMD_3D_MODE_OFF
			if (cnt != 6) break;
			cfg.throt_mode = 0;
			break;
		case 10: // DSHOT_CMD_3D_MODE_ON
			if (cnt != 6) break;
			cfg.throt_mode = 1;
			break;
		case 12: // DSHOT_CMD_SAVE_SETTINGS
			if (cnt != 6) break;
#if defined(AM13E)
            /* Do not block ECAP0 RX/WWDT or BiDShot reply for Flash. */
            if (!dshot_save_running) dshot_save_pending = 1U;
#else
			beepval = savecfg();
#endif
			break;
		case 13: // DSHOT_CMD_EXTENDED_TELEMETRY_ENABLE
			if (cnt != 6) break;
			dshotext = 1;
			dshotval = 0xe00;
			rep = 10;
			break;
		case 14: // DSHOT_CMD_EXTENDED_TELEMETRY_DISABLE
			if (cnt != 6) break;
			dshotext = 0;
			dshotval = 0xeff;
			rep = 10;
			break;
		case 20: // DSHOT_CMD_SPIN_DIRECTION_NORMAL
			if (cnt != 6) break;
			flipdir = 0;
			break;
		case 21: // DSHOT_CMD_SPIN_DIRECTION_REVERSED
			if (cnt != 6) break;
			flipdir = 1;
			break;
#if LED_CNT >= 1
		case 22: // DSHOT_CMD_LED0_ON
			cfg.led |= 1;
			break;
		case 26: // DSHOT_CMD_LED0_OFF
			cfg.led &= ~1;
			break;
#endif
#if LED_CNT >= 2
		case 23: // DSHOT_CMD_LED1_ON
			cfg.led |= 2;
			break;
		case 27: // DSHOT_CMD_LED1_OFF
			cfg.led &= ~2;
			break;
#endif
#if LED_CNT >= 3
		case 24: // DSHOT_CMD_LED2_ON
			cfg.led |= 4;
			break;
		case 28: // DSHOT_CMD_LED2_OFF
			cfg.led &= ~4;
			break;
#endif
#if LED_CNT >= 4
		case 25: // DSHOT_CMD_LED3_ON
			cfg.led |= 8;
			break;
		case 29: // DSHOT_CMD_LED3_OFF
			cfg.led &= ~8;
			break;
#endif
		case 40: // Select motor timing
			if (cnt != 6) break;
			if ((x = (cfg.timing >> 1) + 1) > 15 || x < 8) x = 8;
			cfg.timing = x << 1;
			beepval = x - 7;
			break;
		case 41: // Select PWM frequency
			if (cnt != 6) break;
			if ((x = (cfg.freq_min >> 2) + 1) > 12 || x < 6) x = 6;
			cfg.freq_min = x << 2;
			cfg.freq_max = x << 3;
			beepval = x - 5;
			break;
		case 42: // Select maximum duty cycle ramp
			if (cnt != 6) break;
			if ((x = cfg.duty_ramp / 10 + 1) > 10) x = 0;
			cfg.duty_ramp = x * 10;
			beepval = x;
			break;
		case 43: // Select duty cycle slew rate
			if (cnt != 6) break;
			if ((x = cfg.duty_rate / 10 + 1) > 10) x = 1;
			cfg.duty_rate = x * 10;
			beepval = x;
			break;
		case 47: // Reset settings
			if (cnt != 6) break;
#if defined(AM13E)
            if (!dshot_save_running) dshot_save_pending = 2U;
#else
			beepval = resetcfg();
#endif
			break;
	}
}

/* Portable callback for a physically verified complete DSHOT frame.
 * Transport must resynchronize on a zero result and handle hardware timing.
 */
#if defined(AM13E)
/* Retain the Rel17 eRPM/extended-telemetry selection, inverted CRC and
 * repeat counter. This creates a complete 23-symbol BiDShot TX waveform
 * for the physical PB14 timer/DMA turnaround backend.
 */
void am13e_app_io_bidir_telemetry_levels(uint8_t levels[AM13E_BIDIR_DMA_LEVELS])
{
    if (!dshotval) {
        int value = ertm ? min(ertm, 65408) : 65408;
        int exponent = 0;
        while (value > 511) { value >>= 1; ++exponent; }
        dshotval = value | (exponent << 9);
    }
    const int frame = (dshotval << 4) | dshotcrc(dshotval, 1);
    am13e_bidir_encode_frame((uint16_t)frame, levels);
    if (!rep || !--rep) dshotval = 0;
}

int am13e_app_io_dshot_packet(uint16_t frame, int bidirectional_invert) {
    if(cfg.input_mode!=0)return 0;
    int x = frame;
    if (dshotcrc(x, !!bidirectional_invert)) return 0;
    am13e_app_io_watchdog_feed();
    dshot_apply_packet(x);
    return 1;
}

static int servo_calibration_blocked;
int am13e_app_io_servo_calibration_blocked(void) {
    return servo_calibration_blocked;
}
/* Original servoirq policy; the decoder retains its selected timer domain. */
void am13e_app_io_servo_pulse(unsigned int pulse_us,unsigned int period_ticks) {
    if(cfg.input_mode!=0)return;
#ifdef AM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED
    if(!servo_calibration_finished) {
        servo_capture_period=period_ticks;
        calibirq();
        return; /* Original IRQ handoff: next capture becomes Servo. */
    }

#else
    (void)period_ticks;
    servo_calibration_blocked=!!cfg.throt_cal;
#endif
    if(pulse_us>=28U && pulse_us<=32U) {
        am13e_app_io_watchdog_feed();
        telreq=1;
        return;
    }
    if(pulse_us<800U || pulse_us>2200U) {
        if(ertm)return;
        /* Original idle invalid-signal WWDG reset, with native safe-off
         * ordering; running invalid input must not feed the watchdog.
         */
        am13e_app_service_reset();
    }
    am13e_app_io_watchdog_feed();
    setthrot((int)pulse_us);
}
unsigned am13e_app_io_input_mode(void)
{
    return (unsigned)(uint8_t)cfg.input_mode;
}
int am13e_app_io_service_idle(void) { return ertm==0; }
/* Original Rel17 input_mode=2 binary Serial Command. The stateful
 * policy variables remain those owned by the upstream shared io.c.
 * No fake LED GPIO support is introduced by this native adapter.
 */
int am13e_app_io_serial_frame(const uint8_t *frame,unsigned length,
                              uint8_t *reply,unsigned capacity,unsigned *written) {
    if(cfg.input_mode!=2 || !frame || !reply || !written || capacity<12U ||
       length<2U || length>255U)return 0;
    memcpy(iobuf,frame,length);
    const unsigned before=io_valid_frames;
    const int n=serialfunc((int)length);
    *written=(unsigned)n;
    if(n)memcpy(reply,iobuf,(unsigned)n);
    return io_valid_frames!=before;
}
int am13e_app_io_ibus_frame(const uint8_t *frame,unsigned length) {
    return cfg.input_mode==3 && length==32U && io_core_frame(frame,length,ibusfunc);
}
int am13e_app_io_sbus_frame(const uint8_t *frame,unsigned length) {
    return cfg.input_mode==4 && length==25U && io_core_frame(frame,length,sbusfunc);
}
int am13e_app_io_sbus2_next_slot(uint8_t out[3],unsigned *delay_us) {
    if(cfg.input_mode!=4 || !out || !delay_us || !sbus2_reply_remaining ||
       telphid<1 || telphid>4 || cfg.telem_poles<=0)return 0;
    sbuspacket(sbus2_reply_index);
    memcpy(out,iobuf,3U);
    *delay_us=sbus2_reply_index ? 660U : 3200U;
    ++sbus2_reply_index;--sbus2_reply_remaining;
    return 1;
}
int am13e_app_io_crsf_frame(const uint8_t *frame,unsigned length) {
    return cfg.input_mode==5 && frame && length>=4U && length<=64U &&
           (unsigned)frame[1]+2U==length && io_core_frame(frame,length,crsffunc);
}
int am13e_app_io_exbus_frame(const uint8_t *frame,unsigned length) {
    return cfg.input_mode==6 && frame && length>=8U && length<=64U &&
           frame[2]==length && (unsigned)frame[5]+8U==length &&
           io_core_frame(frame,length,exbusfunc);
}
int am13e_app_io_hott_frame(const uint8_t *frame,unsigned length) {
    return cfg.input_mode==7 && frame && length>=5U && length<=64U &&
           (unsigned)frame[2]*2U+5U==length &&
           ((frame[1]&0x7fU)!=3U || frame[2]>=2U) &&
           io_core_frame(frame,length,hottfunc);
}
unsigned am13e_app_io_crsf_reply(const uint8_t *frame,unsigned length,
                                 uint8_t *reply,unsigned capacity) {
    if(cfg.input_mode!=5 || telmode!=4 || !frame || !reply || capacity<64U ||
       length<4U || length>64U || frame[1]<2U || frame[1]>62U ||
       (unsigned)frame[1]+2U!=length ||
       (frame[2]!=0x28U && frame[2]!=0x2cU && frame[2]!=0x2dU))return 0U;
    /* Called only from preemptible service after CRC acceptance and TX preflight.
     * Keep separate scratch: UART RX can preempt command/config processing.
     */
    static char scratch[1024];
    const unsigned n=crsfresponse((const char *)frame,scratch);
    memcpy(reply,scratch,n);
    return n;
}
unsigned am13e_app_io_exbus_telemetry(uint8_t id,uint32_t device_id,
                                      uint8_t *out,unsigned capacity) {
    if(cfg.input_mode!=6 || !out || capacity<64U || cfg.telem_poles<2)return 0U;
    io_exbus_device_id=device_id;
    const unsigned n=(unsigned)exbusresp(id);
    memcpy(out,iobuf,n);
    return n;
}

#else
void iotim_dma_isr(void) { // DSHOT
	DMA1_IFCR = DMA_IFCR_CTCIF(IOTIM_DMA);
	if (TIM_DIER(IOTIM) & TIM_DIER_UIE) return; // Bad sync
	if (DMA1_CCR(IOTIM_DMA) & DMA_CCR_DIR) {
		dshotreset();
		if (!dshotval) {
			int a = ertm ? min(ertm, 65408) : 65408;
			int b = 0;
			while (a > 511) a >>= 1, ++b;
			dshotval = a | b << 9;
		}
		int a = dshotval << 4 | dshotcrc(dshotval, 1);
		int b=(int)rel17_bidir_gcr((uint16_t)a);
		uint16_t *buf = (uint16_t *)(iobuf + 64);
		buf[0] = -1;
		for (int p = -1, i = 19; i >= 0; --i) {
			if (b >> i & 1) p = ~p;
			buf[20 - i] = p;
		}
		buf[21] = 0;
		buf[22] = 0;
		if (!rep || !--rep) dshotval = 0;
		return;
	}
	if (dshotinv) { // Bidirectional DSHOT
		TIM_CCER(IOTIM) = 0;
		TIM_SMCR(IOTIM) = 0;
		TIM_CCMR1(IOTIM) = 0; // Disable OC before enabling PWM to force OC1REF update (RM: OC1M, note #2)
		TIM_CCMR1(IOTIM) = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM2;
		TIM_CR2(IOTIM) = TIM_CR2_CCDS; // CC1 DMA request on UEV using the same DMA channel
		DMA1_CCR(IOTIM_DMA) = 0;
		DMA1_CMAR(IOTIM_DMA) = (uint32_t)(iobuf + 64);
		DMA1_CNDTR(IOTIM_DMA) = 23;
		DMA1_CCR(IOTIM_DMA) = DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_16BIT | DMA_CCR_MSIZE_16BIT;
		TIM_CCR1(IOTIM) = 0; // Preload high level
		__disable_irq();
		TIM_ARR(IOTIM) = max(CLK_CNT(33333) - TIM_CNT(IOTIM) - 1, 19); // 30us output delay
		TIM_EGR(IOTIM) = TIM_EGR_UG; // Update registers and trigger DMA to preload the first bit
		TIM_EGR(IOTIM); // Ensure UEV has happened
		TIM_ARR(IOTIM) = dshotarr2; // Preload bit time
		TIM_CCER(IOTIM) = TIM_CCER_CC1E; // Enable output
		__enable_irq();
	}
	int x = 0;
	int y = dshotarr1 + 1; // Two bit time
	int z = y >> 2; // Half-bit time
	uint16_t *buf = (uint16_t *)iobuf;
	for (int i = 0; i < 32; i += 2) {
		if (i && buf[i] >= y) { // Invalid pulse timing
			dshotresync();
			return;
		}
		x = x << 1 | (buf[i + 1] >= z);
	}
	if (dshotcrc(x, dshotinv)) { // Invalid checksum
		dshotresync();
		return;
	}
	IWDG_KR = IWDG_KR_RESET;
	dshot_apply_packet(x);
}

void iotim_isr(void) {
	ioirq();
}

#ifdef IO_PA2
void usart2_isr(void) {
	ioirq();
}





static void serialirq(void) {
	if (USART2_CR1 & USART_CR1_TCIE) {
		USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
		DMA1_CCR(USART2_TX_DMA) = 0;
		goto reading;
	}
#ifdef AT32F4
	USART2_SR, USART2_DR; // Clear flags
#else
	USART2_RQR = USART_RQR_RXFRQ; // Clear RXNE
	USART2_ICR = USART_ICR_IDLECF | USART_ICR_ORECF;
#endif
	DMA1_CCR(USART2_RX_DMA) = 0;
	int len = iofunc(sizeof iobuf - DMA1_CNDTR(USART2_RX_DMA));
	if (len) {
		if (len < 0) return;
#ifdef AT32F4
		USART2_SR = ~USART_SR_TC;
#else
		USART2_ICR = USART_ICR_TCCF;
#endif
		USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_TCIE;
		DMA1_CNDTR(USART2_TX_DMA) = len;
		DMA1_CCR(USART2_TX_DMA) = DMA_CCR_EN | DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_8BIT | DMA_CCR_MSIZE_8BIT;
		return;
	}
reading:
	DMA1_CNDTR(USART2_RX_DMA) = sizeof iobuf;
	DMA1_CCR(USART2_RX_DMA) = DMA_CCR_EN | DMA_CCR_MINC | DMA_CCR_PSIZE_8BIT | DMA_CCR_MSIZE_8BIT;
}









static void sbusirq1(void) {
	TIM15_SR = ~TIM_SR_UIF;
	TIM15_DIER = 0;
	TIM15_ARR = -1;
	TIM15_EGR = TIM_EGR_UG;
	USART2_CR1 = USART_CR1_PCE | USART_CR1_M0 | USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
	DMA1_CNDTR(USART2_RX_DMA) = sizeof iobuf;
	DMA1_CCR(USART2_RX_DMA) = DMA_CCR_EN | DMA_CCR_MINC | DMA_CCR_PSIZE_8BIT | DMA_CCR_MSIZE_8BIT;
	ioirq = serialirq;
}

static void sbusirq2(void) {
	static char n;
	TIM15_SR = ~TIM_SR_UIF;
	sbuspacket((unsigned)n);
	DMA1_CNDTR(USART2_TX_DMA) = 3;
	DMA1_CCR(USART2_TX_DMA) = DMA_CCR_EN | DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_8BIT | DMA_CCR_MSIZE_8BIT;
	if (++n < 6) return;
	ioirq = sbusirq1;
	n = 0;
}











static void cliirq(void) {
	static uint16_t i, j;
	int cr = USART2_CR1;
	if (cr & USART_CR1_TXEIE) {
		USART2_TDR = iobuf[i++]; // Clear TXE+TC
		if (i < j) return;
		USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_TCIE;
		i = 0;
		j = 0;
		return;
	}
	if (cr & USART_CR1_TCIE) {
		USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
		return;
	}
	if (USART2_ISR & (USART_ISR_FE | USART_ISR_NF) || i == sizeof iobuf - 1) WWDG_CR = WWDG_CR_WDGA; // Data error
	char b = USART2_RDR; // Clear RXNE
	if (b == '\b' || b == 0x7f) { // Backspace
		if (i) --i;
		return;
	}
	iobuf[i++] = b;
	if (i == 2 && iobuf[0] == 0x00 && iobuf[1] == 0xff) scb_reset_system(); // Reboot into bootloader
	if (b != '\n') return;
	iobuf[i] = '\0';
	i = 0;
	if (!(j = execcmd(iobuf))) return;
	USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_TXEIE;
}
#else
static void cliirq(void) {
	static uint16_t i, j;
	static uint8_t n, b;
	switch (TIM3_DIER) {
		case TIM_DIER_UIE: // Output
			TIM3_SR = ~TIM_SR_UIF;
			if (i < j) {
				int p = -1;
				if (!n++) b = iobuf[i]; // Start bit
				else if (n < 10) { // Data bit
					if (b & 1) p = 0;
					b >>= 1;
				} else { // Stop bit
					if (++i == j) { // End of data
						i = 0;
						j = 0;
					}
					n = 0;
					p = 0;
				}
				TIM3_CCR1 = p;
				break;
			}
			TIM3_SMCR = TIM_SMCR_SMS_RM | TIM_SMCR_TS_TI1F_ED; // Reset on any edge on TI1
			TIM3_CCER = TIM_CCER_CC2E | TIM_CCER_CC2P; // IC2 on falling edge on TI1
			TIM3_SR = ~TIM_SR_CC2IF;
			TIM3_DIER = TIM_DIER_CC2IE;
			TIM3_CCR1 = CLK_CNT(76800); // Half-bit time
			TIM3_EGR = TIM_EGR_UG;
			break;
		case TIM_DIER_CC1IE: // Half-bit time
			TIM3_SR = ~TIM_SR_CC1IF;
			int p = IOTIM_IDR; // Signal level
			if (!n++) { // Start bit
				if (p) WWDG_CR = WWDG_CR_WDGA; // Data error
				b = 0;
				break;
			}
			if (n < 10) { // Data bit
				b >>= 1;
				if (p) b |= 0x80;
				break;
			}
			if (!p || i == sizeof iobuf - 1) WWDG_CR = WWDG_CR_WDGA; // Data error
			TIM3_SR = ~TIM_SR_CC2IF;
			TIM3_DIER = TIM_DIER_CC2IE;
			n = 0;
			if (b == '\b' || b == 0x7f) { // Backspace
				if (i) --i;
				break;
			}
			iobuf[i++] = b;
			if (i == 2 && iobuf[0] == 0x00 && iobuf[1] == 0xff) scb_reset_system(); // Reboot into bootloader
			if (b != '\n') break;
			iobuf[i] = '\0';
			i = 0;
			if (!(j = execcmd(iobuf))) break;
			TIM3_SMCR = 0;
			TIM3_CCR1 = 0; // Preload high level
			TIM3_EGR = TIM_EGR_UG; // Update registers and trigger UEV
			TIM3_CCER = TIM_CCER_CC1E; // Enable output
			TIM3_DIER = TIM_DIER_UIE;
			break;
		case TIM_DIER_CC2IE: // Falling edge
			TIM3_SR = ~(TIM_SR_CC1IF | TIM_SR_CC2IF);
			TIM3_DIER = TIM_DIER_CC1IE;
			break;
	}
}
#endif

#endif /* !AM13E: legacy DMA, UART, software CLI and capture ISRs */

/* The AM13E CLI physical transport handles framing and RX/TX itself;
 * preserve the original shared command parser (src/prog.c).
 */
#if defined(AM13E)
int am13e_app_io_cli_line(char *line) {
    return execcmd(line);
}
#endif

#if defined(AM13E)
/* Rel17 DShot save command deferred from ECAP0 IRQ to lowest-priority
 * PendSV. Normal valid-frame WWDT feeding remains owned by PB14 RX.
 * Repeated commands are coalesced until this transaction completes.
 */
void am13e_app_io_service_pending_save(void)
{
    /* Claim the operation atomically with respect to capture IRQs. */
    const uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (!dshot_save_pending || dshot_save_running ||
        am13e_pb14_bidir_tx_busy()) {
        __set_PRIMASK(mask);
        return; /* preserve current reply and queued request */
    }
    const unsigned operation=dshot_save_pending;
    dshot_save_pending=0U;
    if (ertm || throt) {
        beepval=0; /* do not persist while throttle is requested */
        __set_PRIMASK(mask);
        return;
    }
    dshot_save_running=1U;
    __set_PRIMASK(mask);
    beepval=operation==2U?resetcfg():savecfg();
    dshot_save_running=0U;
}
#endif

#if defined(IO_PA2) || defined(AM13E)
/* Original Core algorithms; only peripheral operations are target seams. */
static uint8_t exbus_description_index;
static int getchan1(const char *buf, int n) { // SBUS/CRSF
	if (n < 0 || n >= 16) return -1;
	int i = (n * 11) >> 3;
	int a = (n * 3) & 7;
	int b = ((n + 1) * 3) & 7;
	int x = a < b ?
		buf[i] >> a | (buf[i + 1] & ((1 << b) - 1)) << (8 - a):
		buf[i] >> a | buf[i + 1] << (8 - a) | (buf[i + 2] & ((1 << b) - 1)) << (16 - a);
	return (x * 5 >> 3) + 880;
}

static int getchan2(const char *buf, int m, int n, int s) { // EXBUS/HoTT
	if (s > 0 && n >= 8 && (n -= s) < 8) return -1;
	if (n < 0 || n >= m) return -1;
	buf += n << 1;
	int a = buf[0];
	int b = buf[1];
	return (s < 0 ? a | b << 8 : a << 8 | b) >> 3;
}

static int serialresp1(int a, int b) {
	iobuf[0] = 4;
	iobuf[1] = a;
	iobuf[2] = b;
	iobuf[3] = crc8(iobuf, 3);
	return 4;
}

static int serialresp2(int x) {
	return serialresp1(x, x >> 8);
}

static int serialfunc(int len) {
	if (len < 2 || iobuf[0] != len || crc8(iobuf, len)) return 0;
	#if defined(AM13E)
    io_feed_watchdog();
#else
	IWDG_KR = IWDG_KR_RESET;
#endif
	len -= 2;
	for (char *buf = iobuf + 1; len--;) {
		int val = *buf++;
		char *pos = buf;
		switch (val) {
			case 0x00: // Throttle
				if (len < 2) return 0;
				val = buf[0] | buf[1] << 8;
				buf += 2;
				if (val & 0x8000) val -= 0x10000; // Propagate sign
				if (val < -2000 || val > 2000) break;
				throt = val;
				break;
			case 0x01: // Drag brake adjustment
				if (len < 1) return 0;
				brake = min(*buf++, 100) * cfg.duty_drag * 41 >> 12;
				auxup = 0;
				break;
			case 0x02: // Normal motor direction
				flipdir = 0;
				break;
			case 0x03: // Reversed motor direction
				flipdir = 1;
				break;
			case 0x04: // LED on/off bits
				if (len < 1) return 0;
				cfg.led = *buf++ & ((1 << LED_CNT) - 1);
				break;
			case 0x80: // Combined telemetry
				iobuf[0] = 12;
				iobuf[1] = temp1;
				iobuf[2] = temp2;
				iobuf[3] = volt;
				iobuf[4] = volt >> 8;
				iobuf[5] = curr;
				iobuf[6] = curr >> 8;
				iobuf[7] = csum;
				iobuf[8] = csum >> 8;
				iobuf[9] = val = min(ertm, 0xffff);
				iobuf[10] = val >> 8;
				iobuf[11] = crc8(iobuf, 11);
				return 12;
			case 0x81: return serialresp2(min(ertm, 0xffff)); // Electrical revolution time (us)
			case 0x82: return serialresp1(temp1, temp2); // Temperature (C)
			case 0x83: return serialresp2(volt); // Voltage (V/100)
			case 0x84: return serialresp2(curr); // Current (A/100)
			case 0x85: return serialresp2(csum); // Consumption (mAh)
			default:
				return 0;
		}
		len -= buf - pos;
	}
	return 0;
}

static int ibusfunc(int len) {
	if (len != 32 || iobuf[0] != 0x20 || iobuf[1] != 0x40) return 0;
	int n1 = cfg.input_ch1;
	int n2 = cfg.input_ch2;
	int x1 = -1;
	int x2 = -1;
	int u = 0xff9f;
	for (int i = 1;; ++i) {
		int j = i << 1;
		int a = iobuf[j];
		int b = iobuf[j + 1];
		int v = a | b << 8;
		if (i == 15) {
			if (u != v) return 0; // Invalid checksum
			break;
		}
		u -= a + b;
		if (i == n1) x1 = v & 0xfff;
		else if (i == n2) x2 = v & 0xfff;
	}
	#if defined(AM13E)
    io_feed_watchdog();
#else
	IWDG_KR = IWDG_KR_RESET;
#endif
	setthrot(x1);
	setbrake(x2);
	return 0;
}

static void sbuspacket(unsigned n) {
	static const char slot[] = {
		0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, // 2..7
		0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, // 10..15
		0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, // 18..23
		0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, // 26..31
	};
	int a = 0, b = 0;
	switch (n) {
		case 0: // SBS-01T (ESC temperature)
			a = temp1 + 100;
			b = a >> 8 | 0x80;
			break;
		case 1: // SBS-01T (motor temperature)
			a = temp2 + 100;
			b = a >> 8 | 0x80;
			break;
		case 2: // SBS-01C (current)
			b = curr;
			a = b >> 8 | 0x40;
			break;
		case 3: // SBS-01C (voltage)
			b = volt;
			a = b >> 8;
			break;
		case 4: // SBS-01C (consumption)
			b = csum;
			a = b >> 8;
			break;
		case 5: // SBS-01R (RPM)
			a = min(erpm / (cfg.telem_poles * 3), 0xffff);
			b = a >> 8;
			break;
	}
	iobuf[0] = slot[n + (telphid - 1) * 6];
	iobuf[1] = a;
	iobuf[2] = b;
}

static int sbusfunc(int len) {
	if (len != 25 || iobuf[0] != 0x0f) return 0;
#if !defined(AM13E)
	TIM15_EGR = TIM_EGR_UG;
#endif
	#if defined(AM13E)
    io_feed_watchdog();
#else
	IWDG_KR = IWDG_KR_RESET;
#endif
	setthrot(getchan1(iobuf + 1, cfg.input_ch1 - 1));
	setbrake(getchan1(iobuf + 1, cfg.input_ch2 - 1));
#if defined(AM13E)
    sbus2_reply_index=0U;
    sbus2_reply_remaining=0U;
#endif
	int a = iobuf[24];
	if ((a & 0xf) != 0x4) return 0; // No telemetry
#if defined(AM13E)
    sbus2_reply_remaining=telphid>=1 && telphid<=4 &&
        !((telmode>=2 && telmode<=5) || a>>4!=telphid-1) ? 6U : 0U;
    return 0;
#else
	if ((telmode >= 2 && telmode <= 5) || a >> 4 != telphid - 1) {
		ioirq = sbusirq1;
		__disable_irq();
		TIM15_ARR = 57279 - TIM15_CNT; // Disable RX for 2000+660*8-120=7160us
		TIM15_EGR = TIM_EGR_UG;
		__enable_irq();
	} else {
		ioirq = sbusirq2;
		__disable_irq();
		TIM15_ARR = 25599 - TIM15_CNT; // Delay TX for 2000+660*2-120=3200us
		TIM15_EGR = TIM_EGR_UG;
		TIM15_EGR; // Ensure UEV has happened
		TIM15_ARR = 5279; // Preload slot interval
		__enable_irq();
	}
	TIM15_SR = ~TIM_SR_UIF;
	TIM15_DIER = TIM_DIER_UIE;
	USART2_CR1 = USART_CR1_PCE | USART_CR1_M0 | USART_CR1_UE | USART_CR1_TE;
	return -1;
#endif
}
static inline __attribute__((always_inline)) unsigned crsfresponse(const char *buf,char *res) {
    int a=buf[1];
    if(telmode!=4 || !(a=min(execcrsfcmd(buf+2,a-1,res+2),61)))return 0U;
#if defined(AM13E)
    if(a<0)return 0U; /* Preserve native failure preflight before any output write. */
#endif
    res[0]=(char)0xc8;res[1]=(char)(a+1);
    res[a+2]=(char)crc8dvbs2(res+2,a);
    return (unsigned)a+3U;
}

static int crsffunc(int len) {
	if (len > 128) return 0; // CSRF physical layer requirement
	#if defined(AM13E)
    for(char *buf=iobuf;len>=4;) {
#else
    for (char *buf = iobuf, *res = buf + len; len >= 4;) {
#endif
		int a = buf[1];
		int b = a + 2;
		if (a < 2 || b > 64 || (len -= b) < 0 || crc8dvbs2(buf + 2, a)) break;
		#if defined(AM13E)
    io_feed_watchdog();
#else
	IWDG_KR = IWDG_KR_RESET;
#endif
		switch (buf[2]) {
			case 0x16: // Channel data
				if (a < 24) break;
				setthrot(getchan1(buf + 3, cfg.input_ch1 - 1));
				setbrake(getchan1(buf + 3, cfg.input_ch2 - 1));
				break;
#ifndef SMALL_DEVICE
			case 0x28: // Ping
			case 0x2c: // Read
			case 0x2d: // Write
#if !defined(AM13E)
                a=(int)crsfresponse(buf,res);
                if(a)sendtelemdata(res,a);
#endif
				break;
#endif
		}
		if (!len) break;
		buf += b;
	}
	return 0;
}

static int exbusresp(int x) {

	#if defined(AM13E)
    int a=(int)io_exbus_device_id;
#else
    int a = DESIG_UNIQUE_ID0;
#endif
	iobuf[0] = 0x3b;
	iobuf[1] = 0x01;
	iobuf[3] = x;
	iobuf[4] = 0x3a;
	iobuf[6] = 0x0f;
	iobuf[8] = a & 0x1f;
	iobuf[9] = 0xa4;
	iobuf[10] = a >> 8;
	iobuf[11] = a >> 16;
	iobuf[12] = 0x00;
	if (exbus_description_index < 7) { // Send description
		static const char *const description_name[] = {"ESCape32", "Temp1", "Temp2", "Volt", "Curr", "Csum", "RPM"};
		static const char *const description_unit[] = {"", "C", "C", "V", "A", "mAh", ""};
		char *p = iobuf + 15;
		char *q = stpcpy(p, description_name[exbus_description_index]);
		int b = q - p; // len1
		p = stpcpy(q, description_unit[exbus_description_index]);
		int c = p - q; // len2
		a = b + c;
		iobuf[2] = a + 18;
		iobuf[5] = a + 10;
		iobuf[7] = a + 8;
		iobuf[13] = exbus_description_index++;
		iobuf[14] = b << 3 | c;
		p[0] = crc8(iobuf + 7, a + 8);
		p[1] = b = crc16ccitt(iobuf, a + 16);
		p[2] = b >> 8;
		return a + 18;
	}
	iobuf[2] = 0x26;
	iobuf[5] = 0x1e;
	iobuf[7] = 0x5c;
	iobuf[13] = 0x11;
	iobuf[14] = temp1;
	iobuf[15] = temp1 >> 8;
	iobuf[16] = 0x21;
	iobuf[17] = temp2;
	iobuf[18] = temp2 >> 8;
	iobuf[19] = 0x34;
	iobuf[20] = volt;
	iobuf[21] = volt >> 8;
	iobuf[22] = volt >> 16 | 0x40;
	iobuf[23] = 0x44;
	iobuf[24] = curr;
	iobuf[25] = curr >> 8;
	iobuf[26] = curr >> 16 | 0x40;
	iobuf[27] = 0x54;
	iobuf[28] = csum;
	iobuf[29] = csum >> 8;
	iobuf[30] = csum >> 16;
	iobuf[31] = 0x64;
	iobuf[32] = a = erpm / (cfg.telem_poles >> 1);
	iobuf[33] = a >> 8;
	iobuf[34] = a >> 16;
	iobuf[35] = crc8(iobuf + 7, 28);
	iobuf[36] = a = crc16ccitt(iobuf, 36);
	iobuf[37] = a >> 8;
	return 38;
}

static int exbusfunc(int len) {
#if !defined(AM13E)
    static uint8_t attempts=10U;
#endif
	for (char *buf = iobuf; len >= 8;) {
		int a = buf[2];
		int b = buf[5];
		if (a != b + 8 || (len -= a) < 0 || crc16ccitt(buf, a)) break;
		#if defined(AM13E)
    io_feed_watchdog();
#else
	IWDG_KR = IWDG_KR_RESET;
#endif
		switch (buf[4]) {
			case 0x31: // Channel data
				setthrot(getchan2(buf + 6, b >> 1, cfg.input_ch1 - 1, -1));
				setbrake(getchan2(buf + 6, b >> 1, cfg.input_ch2 - 1, -1));
#if !defined(AM13E)
                attempts=0U;
#endif
				break;
			case 0x3a: // Telemetry request
				if (len || b) break;
				#if defined(AM13E)
                return 0; /* Formatting/TX happens after transport preflight. */
#else
                return exbusresp(buf[3]);
#endif
		}
		if (!len) return 0;
		buf += a;
	}
#if !defined(AM13E)
    const int rate=exbus_retry_step(&attempts);
    if(rate)USART2_BRR=rate==1 ? CLK_CNT(250000) : CLK_CNT(125000);
#endif
	return 0;
}

static int hottfunc(int len) {
	if (len < 5 || iobuf[0] != 0xa8 || crc16xmodem(iobuf, len - 2) != (iobuf[len - 2] << 8 | iobuf[len - 1])) return 0;
	#if defined(AM13E)
    io_feed_watchdog();
#else
	IWDG_KR = IWDG_KR_RESET;
#endif
	int a = iobuf[2];
	int b = a << 1;
	if (b != len - 5) return 0;
	switch (iobuf[1] & 0x7f) {
		case 1: // SUMDv1
			setthrot(getchan2(iobuf + 3, a, cfg.input_ch1 - 1, 0));
			setbrake(getchan2(iobuf + 3, a, cfg.input_ch2 - 1, 0));
			break;
		case 3: { // SUMDv3
			static const char map[] = {0x60, 0x61, 0x80, 0x82, 0x84, 0x60}; // Function code mapping
			int c = iobuf[b - 1];
			if (c > 5) break;
			int d = map[c];
			int e = d >> 3;
			int f = (d & 7) << 2;
			if ((a -= 2) > e) a = e;
			setthrot(getchan2(iobuf + 3, a, cfg.input_ch1 - 1, f));
			setbrake(getchan2(iobuf + 3, a, cfg.input_ch2 - 1, f));
			break;
        }
	}
	return 0;
}
#endif

#include <stddef.h>

#include "exbus_autobaud.h"
#include <stddef.h>
void am13e_exbus_autobaud_init(AM13E_ExbusAutobaud *s)
{
    if(!s)return;
    s->attempts_remaining=10U;  /* upstream: static char n=10 */
    s->channels_seen=false;
    s->baud=125000U;
}
void am13e_exbus_autobaud_valid(AM13E_ExbusAutobaud *s,
                               bool channel_frame)
{
    if(!s)return;
    if(channel_frame){
        s->attempts_remaining=0U; /* upstream: n=0 on 0x31 */
        s->channels_seen=true;
    }
    /* Valid telemetry/unknown packet does not decrement n.
     * Do not reinterpret this as a signal to reset autobaud.
     */
}
bool am13e_exbus_autobaud_bad(AM13E_ExbusAutobaud *s,
                              uint32_t *requested_baud)
{
    if(!s||!requested_baud||s->channels_seen||
       s->attempts_remaining==0U)return false;
    const int rate=exbus_retry_step(&s->attempts_remaining);
    s->baud=rate==1 ? 250000U : 125000U;
    *requested_baud=s->baud;
    return true;
}


#if defined(AM13E)
#include "cli_entry.h"
#include <stddef.h>
void am13e_cli_entry_select_receiver(AM13E_CliEntry *s)
{
    if(s==NULL)return;
    s->selected=1U;
    s->requested=0U;
}
int am13e_cli_entry_tick(AM13E_CliEntry *s,int high,int edge,int selected)
{
    if(s==NULL)return 0;
    if(selected)am13e_cli_entry_select_receiver(s);
    if(s->selected)return 0;
    if(edge){s->ticks=0U;s->requested=0U;return 0;}
    if(s->requested)return 1;
    if(++s->ticks<AM13E_CLI_ENTRY_TIMEOUT_TICKS)return 0;
    s->ticks=0U;
    /* Rel17 entryirq resets capture counter n on LOW, but does not
     * reset HIGH timeout counter c. Preserve this source detail.
     */
    if(!high)return 0;
    if(!cli_high_timeout(&s->high_timeouts))return 0;
    s->requested=1U;
    return 1;
}

#endif

#if !defined(AM13E) || defined(AM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED) || defined(REL17_CALIBRATION_TEST_ADAPTER)
static void calibirq(void) {
#if defined(AM13E)
#define n servo_calibration_state.count
#define q servo_calibration_state.error
#define x servo_calibration_state.high
#define y servo_calibration_state.low
#else
	static uint8_t n;
	static int16_t q, x, y;
#endif
	#if defined(AM13E)
    int p=(int)servo_capture_period;
#else
	int p = TIM_CCR1(IOTIM); // Pulse period
#endif
	if (cfg.throt_cal && // 50/100/125/200/250/333/500Hz 11/22ms servo PWM within 8% margin
		((p < 22880 && p > 21120) || (p < 20800 && p > 19200) || (p < 11440 && p > 10560) || (p < 10400 && p > 9600) || (p < 8320 && p > 7680) ||
		(p < 5200 && p > 4800) || (p < 4160 && p > 3840) || (p < 3120 && p > 2880) || (p < 2080 && p > 1920))) {
		#if defined(AM13E)
        am13e_app_io_watchdog_feed();
#else
		IWDG_KR = IWDG_KR_RESET;
#endif
		q += p - ((p + 500) / 1000) * 1000; // Cumulative error
		if (++n & 3) return;
		if (q > x) { // Slow down
			y = -q;
			q = 0;
			#if defined(AM13E)
        am13e_app_servo_clock_trim(-1);
#else
			hsictl(-1);
#endif
			return;
		}
		if (q < y) { // Speed up
			x = -q;
			q = 0;
			#if defined(AM13E)
        am13e_app_servo_clock_trim(1);
#else
			hsictl(1);
#endif
			return;
		}
	}
#if defined(AM13E)
    servo_calibration_finished=1;
#else
	ioirq = servoirq;
	TIM_CCMR1(IOTIM) = TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_DTF_DIV_8_N_8 | TIM_CCMR1_CC2S_IN_TI1 | TIM_CCMR1_IC2F_DTF_DIV_8_N_8;
	TIM_CCER(IOTIM) = TIM_CCER_CC2E | TIM_CCER_CC2P; // IC2 on falling edge on TI1
	TIM_SR(IOTIM) = ~TIM_SR_CC2IF;
	TIM_DIER(IOTIM) = TIM_DIER_CC2IE;
#endif
}
#if defined(AM13E)
#undef n
#undef q
#undef x
#undef y
#endif
#endif
