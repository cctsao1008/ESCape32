/* Original src/io.c::exbusfunc()/getchan2(), no STM32 DMA.
 * a = buf[2] == buf[5] + 8, 8<=a<=64; original crc16ccitt
 * returns residue zero over the complete packet. 0x31 carries
 * little-endian 16-bit channels, >>3. 0x3a requests telemetry.
 */
#include "exbus_receiver.h"
#include <stddef.h>
static int channel_us(const uint8_t *data,unsigned channels,unsigned selected)
{
    if(selected==0U||selected>channels)return -1;
    const unsigned i=(selected-1U)*2U;
    return (int)((data[i]|((unsigned)data[i+1U]<<8U))>>3U);
}
bool am13e_exbus_decode_frame(const uint8_t *p,unsigned len,
    unsigned ch1,unsigned ch2,AM13E_ExbusCrc16 crc,
    int *throttle,int *brake,bool *has_channels,bool *telemetry_request)
{
    if(!p||!crc||!throttle||!brake||!has_channels||
       !telemetry_request||len<8U||len>64U||
       p[2]!=len||(unsigned)p[5]+8U!=len||
       crc((const char *)p,(int)len)!=0U)return false;
    *throttle=-1;
    *brake=-1;
    *has_channels=false;
    *telemetry_request=false;
    if(p[4]==0x31U){
        const unsigned channels=p[5]>>1U;
        *throttle=channel_us(p+6U,channels,ch1);
        *brake=channel_us(p+6U,channels,ch2);
        *has_channels=true;
    } else if(p[4]==0x3aU&&p[5]==0U){
        *telemetry_request=true;
    }
    return true;
}
