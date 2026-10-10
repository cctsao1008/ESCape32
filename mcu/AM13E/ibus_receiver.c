/* Original src/io.c ibusfunc() checksum/indices, without STM32 USART.
 * Board-selected UART/DMA supplies exactly 32 received bytes.
 */
#include "ibus_receiver.h"
#include <stddef.h>

bool am13e_ibus_decode_channels(const uint8_t *p,unsigned length,
                                unsigned ch1,unsigned ch2,
                                int *throttle_us,int *brake_us)
{
    if(!p||!throttle_us||!brake_us||length!=32U||
       p[0]!=0x20U||p[1]!=0x40U)return false;
    unsigned sum=0xff9fU;
    int x1=-1,x2=-1;
    for(unsigned i=1U;i<=14U;++i){
        const unsigned j=i<<1U;
        const unsigned a=p[j],b=p[j+1U],v=a|(b<<8U);
        sum-=a+b;
        if(i==ch1)x1=(int)(v&0xfffU);
        else if(i==ch2)x2=(int)(v&0xfffU);
    }
    if(sum!=(unsigned)(p[30]|(p[31]<<8U)))return false;
    *throttle_us=x1;
    *brake_us=x2;
    return true;
}
