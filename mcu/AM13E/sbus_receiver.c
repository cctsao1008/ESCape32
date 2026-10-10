/* Original Rel17 src/io.c getchan1()/sbusfunc() semantics:
 * exactly 25 bytes, 0x0f header, 16 packed 11-bit channels, output
 * (raw*5>>3)+880 microseconds. The original SBUS2 last-byte slot
 * scheduling is NOT part of this receive-only adapter.
 */
#include "sbus_receiver.h"
#include <stddef.h>

static int getchan(const uint8_t *data,unsigned ch)
{
    if(ch>=16U)return -1;
    const unsigned start=ch*11U;
    unsigned raw=0U;
    for(unsigned b=0U;b<11U;++b){
        const unsigned i=start+b;
        if(data[i>>3U] & (1U << (i&7U)))raw|=1U<<b;
    }
    return (int)((raw*5U>>3U)+880U);
}
bool am13e_sbus_decode_channels(const uint8_t *p,unsigned len,
                                unsigned throttle_channel,
                                unsigned brake_channel,
                                int *throttle_us,int *brake_us)
{
    if(!p||!throttle_us||!brake_us||len!=25U||p[0]!=0x0fU)
        return false;
    *throttle_us=(throttle_channel>=1U&&throttle_channel<=16U)?
        getchan(p+1U,throttle_channel-1U):-1;
    *brake_us=(brake_channel>=1U&&brake_channel<=16U)?
        getchan(p+1U,brake_channel-1U):-1;
    return true;
}
