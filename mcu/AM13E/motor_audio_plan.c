#include "motor_audio_plan.h"
#include <stddef.h>
/* Directly from Rel17 util.c::playmusic(), base 8MHz TIM1 tone table.
 * AM13E uses actual 100MHz MCPWM BUSCLK /16=6.25MHz for audio, so
 * the physical period/pulse counts scale by 25/32, preserving pitch
 * and note duty fraction (without assuming unverified DEAD_TIME).
 */
static const uint16_t rel17_arr[13]={
    15287,14429,13619,12856,12133,11452,10810,
    10203,9630,9090,8579,8097,7643
};
int am13e_audio_tone(int semitone,int octave,int volume,AM13E_AudioTone *out)
{
    if (!out || semitone<0 || semitone>12 ||
        octave<0 || octave>10 || volume<=0 || volume>255) return 0;
    const uint32_t legacy_ticks=(rel17_arr[semitone]>>octave)+1U;
    const uint32_t ticks=(legacy_ticks*25U+16U)/32U;
    const uint32_t compare=((uint32_t)volume*25U+16U)/32U;
    if(ticks<3U || ticks>65536U || compare>=ticks || compare==0U) return 0;
    out->period=(uint16_t)(ticks-1U);
    out->compare=(uint16_t)compare;
    return 1;
}
int am13e_audio_pcm(int8_t sample,int volume,AM13E_AudioPCM *out)
{
    if (!out || volume<=0 || volume>255) return 0;
    /* Original Rel17 util.c:
     * CCR1=DEAD_TIME + ((x+128)*vol*CLK_MHZ >> 13)
     * CCR3=DEAD_TIME + ((127-x)*vol*CLK_MHZ >> 13)
     * 168MHz TIM1 -> 100MHz MCPWM, both 24kHz PWM carrier.
     * DEAD_TIME cannot be inferred from board pin assignment.
     */
    const uint32_t u=((uint32_t)(sample+128)*volume*100U)>>13U;
    const uint32_t w=((uint32_t)(127-sample)*volume*100U)>>13U;
    if(u>=4166U || w>=4166U) return 0;
    out->u=(uint16_t)u;
    out->w=(uint16_t)w;
    return 1;
}
