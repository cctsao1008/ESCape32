/* ESCape32 Rel17 Music and AU/PCM on the MOTOR MCPWM0.
 * Never a standalone buzzer GPIO. Score/PCM parser and blocking delay()
 * remain unchanged in src/util.c; this layer owns note/rate arithmetic
 * and delegates EXCLUSIVE physical register programming to motor_safety.
 *
 * PB14 DShot (ECAP0/TIMG4) continues operating during playback.
 * TIMG12 (motor commutation) is loaned to PCM sample clock ONLY while
 * Motor/Audio owner == PCM. SW forced LOW + six GPIO Input pads remain
 * applied until a separate qualified power-stage arming path exists.
 */
#include "util_backend.h"
#include "motor_audio_hw.h"
#include "motor_backend.h" /* Real latched fault shutdown, never a stub */
#include "motor_audio_plan.h"
#include "motor_event_timer.h"
#include <stdint.h>
#include <soc.h>

enum { AUDIO_NONE=0,AUDIO_MUSIC=1,AUDIO_PCM=2 };
enum { MCPWM_CARRIER_HZ=24000 };
static volatile unsigned current_audio;
static uint16_t note_volume_ticks;
static uint16_t last_music_tbctr;
static uint8_t music_u_active;
static uint32_t pcm_sample_rate;
static int pcm_volume;

static void audio_fault(void)
{
    /* Shared motor faults are nonrecoverable; never carry on as if
     * an unavailable motor-generated sound had succeeded.
     */
    am13e_app_motor_fault_shutdown();
    am13e_app_motor_fault_reset();
}

void am13e_app_audio_music_begin(void)
{
    if(current_audio) audio_fault();
    am13e_app_motor_audio_begin(AUDIO_MUSIC);
    current_audio=AUDIO_MUSIC;
    note_volume_ticks=0U;
    music_u_active=0U;
    /* Initial pause before first note remains a real all-zero AQ image.
     * The subsequent note reprograms the oscillator carrier period.
     */
    am13e_app_motor_audio_period(4165U);
    am13e_app_motor_audio_compare(0U,0U);
    last_music_tbctr=am13e_app_motor_audio_counter();
}

void am13e_app_audio_music_note(int semitone,int octave_shift,int volume)
{
    if(current_audio!=AUDIO_MUSIC) audio_fault();
    AM13E_AudioTone tone;
    if(!am13e_audio_tone(semitone,octave_shift,volume,&tone)) audio_fault();
    am13e_app_motor_audio_period(tone.period);
    note_volume_ticks=tone.compare;
    music_u_active=1U;
    am13e_app_motor_audio_compare(tone.compare,0U);
    last_music_tbctr=am13e_app_motor_audio_counter();
}

void am13e_app_audio_music_pause(void)
{
    if(current_audio!=AUDIO_MUSIC) audio_fault();
    note_volume_ticks=0U;
    am13e_app_motor_audio_compare(0U,0U);
    last_music_tbctr=am13e_app_motor_audio_counter();
}

void am13e_app_audio_music_tick(void)
{
    /* delayf() is also used outside playmusic(). That is legal;
     * only an active MUSIC owner may touch audio compare registers.
     */
    if(current_audio!=AUDIO_MUSIC) return;
    const uint16_t now=am13e_app_motor_audio_counter();
    /* TIM1 UIF behavior: alternate CCR1 and CCR3 each full tone
     * period; the counter wraps from TBPRD to zero. Polling must be
     * checked against actual silicon jitter in later validation.
     */
    if(note_volume_ticks && now<last_music_tbctr){
        music_u_active^=1U;
        am13e_app_motor_audio_compare(
            music_u_active?note_volume_ticks:0U,
            music_u_active?0U:note_volume_ticks);
    }
    last_music_tbctr=now;
}

void am13e_app_audio_pcm_begin(uint32_t rate_hz,int volume)
{
    if(current_audio || rate_hz<1000U || rate_hz>48000U ||
       volume<=0 || volume>255) audio_fault();
    am13e_app_motor_audio_begin(AUDIO_PCM);
    current_audio=AUDIO_PCM;
    pcm_volume=volume;
    pcm_sample_rate=rate_hz;
    /* The original 24kHz TIM1 PWM carrier becomes MCPWM0.
     * 100MHz / 24000Hz = 4166 ticks, no invented GPIO tone.
     */
    am13e_app_motor_audio_period((uint16_t)(100000000U/MCPWM_CARRIER_HZ-1U));
    am13e_app_motor_audio_compare(0U,0U);
    am13e_app_motor_audio_pcm_clock_begin(rate_hz);
}

void am13e_app_audio_pcm_sample(int8_t sample)
{
    if(current_audio!=AUDIO_PCM || pcm_sample_rate==0U) audio_fault();
    AM13E_AudioPCM wave;
    if(!am13e_audio_pcm(sample,pcm_volume,&wave)) audio_fault();
    /* Sample cadence is hardware TIMG12, NOT the 16kHz SysTick;
     * up to 48kHz AU data can be paced without fake sample drops.
     */
    am13e_app_motor_audio_pcm_sample_wait();
    am13e_app_motor_audio_compare(wave.u,wave.w);
}

void am13e_app_audio_end(void)
{
    if(current_audio!=AUDIO_MUSIC && current_audio!=AUDIO_PCM)
        audio_fault();
    if(current_audio==AUDIO_PCM)
        am13e_app_motor_audio_pcm_clock_end();
    am13e_app_motor_audio_end();
    pcm_sample_rate=0U;
    pcm_volume=0;
    note_volume_ticks=0U;
    current_audio=AUDIO_NONE;
}
