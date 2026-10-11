/* Original Rel17 src/io.c::hottfunc/getchan2:
 * 0xa8 header, count at [2], (count*2)+5 bytes,
 * big-endian CRC16-XMODEM over all but last 2 bytes, CRC big-endian.
 * SUMDv1 type 1 uses direct 16-bit big-endian channel >>3.
 * SUMDv3 type 3 uses the exact original function-code map
 * (0x60,0x61,0x80,0x82,0x84,0x60), n/s remapping.
 */
#include "hott_receiver.h"
#include <stddef.h>
static int channel(const uint8_t *data,int channels,int n,int s)
{
    if(s>0&&n>=8&&(n-=s)<8)return -1;
    if(n<0||n>=channels)return -1;
    const unsigned idx=(unsigned)n<<1U;
    return (int)((((unsigned)data[idx]<<8U)|data[idx+1U])>>3U);
}
bool am13e_hott_decode_frame(const uint8_t *p,unsigned len,
    unsigned ch1,unsigned ch2,AM13E_HottCrc16 crc,
    int *throttle_us,int *brake_us,bool *has_channels)
{
    if(!p||!crc||!throttle_us||!brake_us||!has_channels||
       len<5U||len>64U||p[0]!=0xa8U||
       (unsigned)p[2]*2U+5U!=len ||
       crc((const char *)p,(int)(len-2U))!=
           (uint16_t)(((uint16_t)p[len-2U]<<8U)|p[len-1U]))
        return false;
    *throttle_us=-1;
    *brake_us=-1;
    *has_channels=false;
    if((p[1]&0x7fU)==1U){
        *throttle_us=channel(p+3U,(int)p[2],(int)ch1-1,0);
        *brake_us=channel(p+3U,(int)p[2],(int)ch2-1,0);
        *has_channels=true;
    } else if((p[1]&0x7fU)==3U && p[2]>=2U){
        static const uint8_t map[]={0x60U,0x61U,0x80U,
                                   0x82U,0x84U,0x60U};
        const unsigned c=p[(unsigned)p[2]*2U-1U];
        if(c>=sizeof map)return true; /* Rel17 ignores bad map. */
        const unsigned d=map[c],s=(d&7U)<<2U;
        int m=(int)p[2]-2;
        const int limit=(int)(d>>3U);
        if(m>limit)m=limit;
        *throttle_us=channel(p+3U,m,(int)ch1-1,(int)s);
        *brake_us=channel(p+3U,m,(int)ch2-1,(int)s);
        *has_channels=true;
    }
    return true;
}
