/* Rel17 TIM1 -> AM13E MCPWM0 Motor Audio arithmetic.
 * Pure host-testable calculations; actual timing/output is motor_audio.c.
 */
#pragma once
#include <stdint.h>
typedef struct {
    uint16_t period; /* Real MCPWM0 TBPRD: 100MHz/16 for score */
    uint16_t compare; /* Rel17 volume pulse, scaled from TIM1 8MHz */
} AM13E_AudioTone;
typedef struct {
    uint16_t u;
    uint16_t w; /* TIM1 CCR1/CCR3 concept, never beeper GPIO */
} AM13E_AudioPCM;
int am13e_audio_tone(int semitone,int octave,int volume,AM13E_AudioTone *out);
int am13e_audio_pcm(int8_t sample,int volume,AM13E_AudioPCM *out);
