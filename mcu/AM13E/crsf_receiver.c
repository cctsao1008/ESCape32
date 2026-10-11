/* Mirrors original src/io.c::crsffunc/getchan1 and
 * src/util.c::crc8dvbs2, not a new protocol or throttle policy.
 * Length byte 2..62, complete frame 4..64 bytes, CRC8-DVB-S2.
 */
#include "crsf_receiver.h"
#include <stddef.h>
static uint8_t dvbs2_residue(const uint8_t *p,unsigned n)
{
    uint8_t crc=0U;
    for(unsigned i=0U;i<n;++i){
        crc^=p[i];
        for(unsigned j=0U;j<8U;++j)
            crc=(crc&0x80U)?
                (uint8_t)((crc<<1U)^0xd5U):(uint8_t)(crc<<1U);
    }
    return crc;
}
static int getchan1_us(const uint8_t *data,unsigned n)
{
    if(n==0U||n>16U)return -1;
    const unsigned start=(n-1U)*11U;
    unsigned x=0U;
    for(unsigned i=0U;i<11U;++i){
        const unsigned bit=start+i;
        if(data[bit>>3U]&(1U<<(bit&7U)))x|=1U<<i;
    }
    return (int)((x*5U>>3U)+880U);
}
bool am13e_crsf_decode_frame(const uint8_t *p,unsigned n,
                             unsigned ch1,unsigned ch2,
                             int *throttle_us,int *brake_us,
                             bool *has_channels)
{
    if(!p||!throttle_us||!brake_us||!has_channels||
       n<4U||n>64U||p[1]<2U||p[1]>62U||
       (unsigned)p[1]+2U!=n||
       dvbs2_residue(p+2U,p[1])!=0U)return false;
    *has_channels=false;
    *throttle_us=-1;
    *brake_us=-1;
    if(p[2]==0x16U&&p[1]>=24U){
        *throttle_us=getchan1_us(p+3U,ch1);
        *brake_us=getchan1_us(p+3U,ch2);
        *has_channels=true;
    }
    /* Rel17 feeds its watchdog on every CRC-valid frame, even
     * unhandled CRSF command types. Only 0x16 changes throttle.
     */
    return true;
}
