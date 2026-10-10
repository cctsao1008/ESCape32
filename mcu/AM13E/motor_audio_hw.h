/* Motor Audio exclusive hardware owner; no independent beeper GPIO.
 * 1 = score/music, 2 = AU/PCM. Motor pads remain GPIO INPUT and
 * AQ software-forced LOW until board-level gate/Trip qualification.
 */
#pragma once
#ifndef AM13E
#error "AM13E Audio shared boundary"
#endif
#include <stdint.h>
void am13e_app_motor_audio_begin(int mode);
void am13e_app_motor_audio_period(uint16_t period);
void am13e_app_motor_audio_compare(uint16_t phase_u,uint16_t phase_w);
uint16_t am13e_app_motor_audio_counter(void);
void am13e_app_motor_audio_end(void);
