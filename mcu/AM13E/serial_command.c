#include "serial_command.h"
#include <stddef.h>
static unsigned response2(uint8_t *out,int x,AM13E_SerialCrc8 crc)
{
    out[0]=4U;out[1]=(uint8_t)x;out[2]=(uint8_t)(x>>8);
    out[3]=crc((const char *)out,3);
    return 4U;
}
bool am13e_serial_command(const uint8_t *p,unsigned n,
                          AM13E_SerialCommandState *s,
                          uint8_t *reply,unsigned capacity,
                          unsigned *written,AM13E_SerialCrc8 crc)
{
    if(!p||!s||!reply||!written||!crc||n<2U||n>255U||
       p[0]!=n||capacity<12U||
       crc((const char *)p,(int)n)!=0U)return false;
    *written=0U;
    const unsigned end=n-1U; /* final byte is incoming CRC8 */
    for(unsigned pos=1U;pos<end;){
        const uint8_t opcode=p[pos++];
        switch(opcode){
          case 0x00U: { /* Signed throttle [-2000,2000], native Rel17 units */
            if(pos+2U>end)return true;
            int val=(int)(p[pos]|((unsigned)p[pos+1U]<<8U));pos+=2U;
            if(val&0x8000)val-=0x10000;
            if(val>=-2000&&val<=2000)s->throttle=val;
            break;
          }
          case 0x01U:
            if(pos+1U>end)return true;
            {unsigned b=p[pos++];if(b>100U)b=100U;
             s->brake=((int)b*s->duty_drag*41)>>12;
             s->auxup=0;}
            break;
          case 0x02U:s->flipdir=0;break;
          case 0x03U:s->flipdir=1;break;
          case 0x04U:
            if(pos+1U>end)return true;
            if(s->led_count>0&&s->led_count<31)
                s->led=p[pos]&((1U<<s->led_count)-1U);
            ++pos;
            break;
          case 0x80U:{
            int v=s->ertm;if(v>65535)v=65535;
            reply[0]=12U;
            reply[1]=(uint8_t)s->temp1;
            reply[2]=(uint8_t)s->temp2;
            reply[3]=(uint8_t)s->volt;
            reply[4]=(uint8_t)(s->volt>>8);
            reply[5]=(uint8_t)s->curr;
            reply[6]=(uint8_t)(s->curr>>8);
            reply[7]=(uint8_t)s->csum;
            reply[8]=(uint8_t)(s->csum>>8);
            reply[9]=(uint8_t)v;
            reply[10]=(uint8_t)(v>>8);
            reply[11]=crc((const char *)reply,11);
            *written=12U;return true;
          }
          case 0x81U:{
            int v=s->ertm;if(v>65535)v=65535;
            *written=response2(reply,v,crc);return true;
          }
          case 0x82U:
            reply[0]=4U;reply[1]=(uint8_t)s->temp1;
            reply[2]=(uint8_t)s->temp2;
            reply[3]=crc((const char *)reply,3);
            *written=4U;return true;
          case 0x83U:*written=response2(reply,s->volt,crc);return true;
          case 0x84U:*written=response2(reply,s->curr,crc);return true;
          case 0x85U:*written=response2(reply,s->csum,crc);return true;
          default:return true; /* upstream silent unknown opcode */
        }
    }
    return true;
}
