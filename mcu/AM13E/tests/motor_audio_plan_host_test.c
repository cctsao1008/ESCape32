/* Actual Rel17 score and PCM amplitude arithmetic, host regression. */
#include "motor_audio_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    AM13E_AudioTone tone;
    AM13E_AudioPCM pcm;
    assert(!am13e_audio_tone(0,0,50,NULL));
    assert(!am13e_audio_tone(-1,0,50,&tone));
    assert(!am13e_audio_tone(13,0,50,&tone));
    assert(!am13e_audio_tone(0,-1,50,&tone));
    assert(!am13e_audio_tone(0,11,50,&tone));
    for(int octave=0;octave<=10;++octave)
        for(int note=0;note<=12;++note)
            for(int volume=1;volume<=100;++volume) {
                if(am13e_audio_tone(note,octave,volume,&tone))
                    assert(tone.compare>0U && tone.compare<=tone.period);
            }
    assert(am13e_audio_tone(0,0,50,&tone));
    assert(tone.period==11943U); /* C: original (15287+1)*25/32 */
    assert(tone.compare==39U);
    for(int x=-128;x<=127;++x)
       for(int volume=1;volume<=100;++volume){
           assert(am13e_audio_pcm((int8_t)x,volume,&pcm));
           assert(pcm.u<4166U && pcm.w<4166U);
           assert(pcm.u==(uint16_t)(((uint32_t)(x+128)*volume*100U)>>13));
           assert(pcm.w==(uint16_t)(((uint32_t)(127-x)*volume*100U)>>13));
       }
    assert(am13e_audio_pcm(-128,100,&pcm) && pcm.u==0U);
    assert(am13e_audio_pcm(127,100,&pcm) && pcm.w==0U);
    puts("PASS: 16k+ Rel17 tone plans, 25k+ PCM sample/volume plans");
    return 0;
}
