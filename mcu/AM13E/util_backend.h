/*
 * AM13E ESCape32 Rel17 Application utility hardware contract.
 *
 * Peripheral functions are ported in the AM13E backend; no synthetic
 * STM32 registers, assumed gate polarity or link-only dummy callbacks.
 * Flash persistence is implemented by cfg_flash_runtime.c using real
 * TI DL_Flash, with the strict 4KiB FW1-only partition and RAMFUNC
 * runtime/linker qualification still required.
 *
 * GPIO, LED and oscillator routines (initgpio/initled/ledctl/hsictl)
 * remain declared in common.h and require an AM13E board implementation.
 *
 * Config persistence: destination is the image's linker-provided _cfg
 * address, source is the in-RAM serialized settings region
 * [_cfg_start, _cfg_end). Do not blindly erase or program this region:
 * flash partition, alignment, ECC, power-loss behavior, IRQ/RAM execution
 * and write-protection are hardware-qualification items.
 * Return nonzero ONLY after erase+program+readback completed successfully.
 *
 * Commutation reset must put motor bridge in its hardware-defined safe
 * state; no fake implementation is allowed.
 *
 * Audio: the Rel17 score parser and AU/PCM decoder remain in util.c.
 * The backend owns all PWM/timer programming and proper pause/timing.
 * music_note takes semitone index 0..12 and octave shift, not raw STM32
 * TIM1 ARR counts. music_tick preserves legacy software commutation
 * of the audio outputs during delay().
 * pcm_sample must pace samples at the configured rate (blocking or
 * equivalent completed playback); no silent drop is permitted.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E Application utility backend is TI-only"
#endif

#include <stdint.h>

int am13e_app_cfg_commit(const void *destination, const void *source,
                         unsigned int byte_count);
void am13e_app_commutation_reset(void);

void am13e_app_audio_music_begin(void);
void am13e_app_audio_music_note(int semitone, int octave_shift, int volume);
void am13e_app_audio_music_pause(void);
void am13e_app_audio_music_tick(void);
void am13e_app_audio_pcm_begin(uint32_t rate_hz, int volume);
void am13e_app_audio_pcm_sample(int8_t sample);
void am13e_app_audio_end(void);
