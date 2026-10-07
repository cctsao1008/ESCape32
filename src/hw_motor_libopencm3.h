/*
** Existing ESCape32 libopencm3 motor backend.
**
** This file intentionally preserves the exact STM32/GD32/AT32 register
** sequences that previously lived in src/main.c. The functions are always
** inlined because they execute in the commutation hot path.
*/

#pragma once

static inline __attribute__((always_inline))
void hw_motor_sine_update(int period, int phase_a, int phase_b, int phase_c)
{
	TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE | TIM_CR1_UDIS;
	TIM1_ARR = period;
	TIM1_CCR1 = phase_a;
	TIM1_CCR2 = phase_b;
	TIM1_CCR3 = phase_c;
	TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
}

static inline __attribute__((always_inline))
void hw_motor_sine_enable_outputs(void)
{
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 |
		TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;

#ifdef PWM_ENABLE
	int er = TIM_CCER_CC1E | TIM_CCER_CC1NP |
		TIM_CCER_CC2E | TIM_CCER_CC2NP |
		TIM_CCER_CC3E | TIM_CCER_CC3NP;
#else
	int er = TIM_CCER_CC1E | TIM_CCER_CC1NE |
		TIM_CCER_CC2E | TIM_CCER_CC2NE |
		TIM_CCER_CC3E | TIM_CCER_CC3NE;
#endif

#ifdef INVERTED_HIGH
	er |= TIM_CCER_CC1P | TIM_CCER_CC2P | TIM_CCER_CC3P;
#endif

	TIM1_CCER = er;
}

static inline __attribute__((always_inline))
void hw_motor_apply_six_step(int positive, int negative, int damp)
{
	int m1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;

#ifdef TIM1_CCR5
	int m2 = TIM_CCMR2_OC3PE;
	int er = TIM_CCER_CC5E;
#else
	int m2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_PWM1;
	int er = TIM_CCER_CC4E;
#endif

	if (positive & 1) {
		m1 |= TIM_CCMR1_OC1M_PWM1;
#ifdef PWM_ENABLE
		er |= TIM_CCER_CC1E;
#else
		er |= damp ? TIM_CCER_CC1E | TIM_CCER_CC1NE : TIM_CCER_CC1E;
#endif
	} else if (negative & 1) {
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

	if (positive & 2) {
		m1 |= TIM_CCMR1_OC2M_PWM1;
#ifdef PWM_ENABLE
		er |= TIM_CCER_CC2E;
#else
		er |= damp ? TIM_CCER_CC2E | TIM_CCER_CC2NE : TIM_CCER_CC2E;
#endif
	} else if (negative & 2) {
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

	if (positive & 4) {
		m2 |= TIM_CCMR2_OC3M_PWM1;
#ifdef PWM_ENABLE
		er |= TIM_CCER_CC3E;
#else
		er |= damp ? TIM_CCER_CC3E | TIM_CCER_CC3NE : TIM_CCER_CC3E;
#endif
	} else if (negative & 4) {
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
}

static inline __attribute__((always_inline))
void hw_motor_set_idle_pwm_mode(void)
{
#ifdef PWM_ENABLE
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM2 |
		TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM2;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM2;
#else
	TIM1_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 |
		TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM1_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1;
#endif
}

static inline __attribute__((always_inline))
void hw_motor_commit_update(void)
{
	TIM1_EGR = TIM_EGR_UG | TIM_EGR_COMG;
}
