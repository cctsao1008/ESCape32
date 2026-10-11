/* TIMG12 sole owner: Rel17 sine and BEMF commutation one-shot. */
#include <stdint.h>
#pragma once
#if !defined(AM13E)
#error "AM13E motor timer only"
#endif
void am13e_app_motor_timing_init(void);
/* Physical TIMG12 one-shot cancellation; not comparator routing disable. */
void am13e_app_motor_timing_cancel(void);
void TIMG12_0_IRQHandler(void);

/* Audio PCM owns TIMG12 only while the motor is quiesced. TIMG4 is
 * never borrowed: it belongs exclusively to PB14 BiDShot TX.
 */
void am13e_app_motor_audio_pcm_clock_begin(uint32_t sample_rate_hz);
void am13e_app_motor_audio_pcm_sample_wait(void);
void am13e_app_motor_audio_pcm_clock_end(void);
