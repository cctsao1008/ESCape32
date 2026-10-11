/* Frame envelope is production code; original execcrsfcmd/CRC
 * callbacks replaced solely within Host fixture to verify transport
 * wrapper semantics and bound safety.
 */
#include "crsf_command_reply.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,reply_len=3U;
static int execute(const char *command,int len,char *reply){
    ++calls;
    assert((uint8_t)command[0]==0x28U ||
           (uint8_t)command[0]==0x2cU ||
           (uint8_t)command[0]==0x2dU);
    assert(len==3);
    for(unsigned i=0;i<reply_len;++i)reply[i]=(char)(0x21U+i);
    return (int)reply_len;
}
static uint8_t crc_reference(const char *bytes,int n){
    uint8_t c=0U;
    for(int i=0;i<n;++i){
        c^=(uint8_t)bytes[i];
        for(int j=0;j<8;++j)
            c=(uint8_t)((c<<1U)^((c&0x80U)?0xd5U:0U));
    }
    return c;
}
int main(void){
    uint8_t request[6]={0xc8U,4U,0x28U,0x00U,0x91U,0};
    uint8_t out[64]={0};
    char workspace[1024]={0};
    unsigned n=am13e_crsf_reply_envelope(request,sizeof request,
                       out,sizeof out,workspace,sizeof workspace,
                       execute,crc_reference);
    assert(n==6U&&calls==1U);
    assert(out[0]==0xc8U&&out[1]==4U);
    assert(out[2]==0x21U&&out[3]==0x22U&&out[4]==0x23U);
    assert(out[5]==crc_reference((const char *)out+2,3));
    assert(!am13e_crsf_reply_envelope(NULL,6U,out,64U,workspace,1024U,execute,crc_reference));
    assert(!am13e_crsf_reply_envelope(request,6U,out,63U,workspace,1024U,execute,crc_reference));
    assert(!am13e_crsf_reply_envelope(request,6U,out,64U,workspace,1023U,execute,crc_reference));
    assert(calls==1U); /* no command mutation before output preflight */
    uint8_t invalid[6];memcpy(invalid,request,6U);
    invalid[1]=62U;
    assert(!am13e_crsf_reply_envelope(invalid,6U,out,64U,workspace,1024U,execute,crc_reference));
    invalid[1]=4U;invalid[2]=0x16U;
    assert(!am13e_crsf_reply_envelope(invalid,6U,out,64U,workspace,1024U,execute,crc_reference));
    assert(calls==1U);
    reply_len=120U;
    n=am13e_crsf_reply_envelope(request,6U,out,64U,workspace,1024U,execute,crc_reference);
    assert(n==64U&&calls==2U&&out[1]==62U);
    assert(out[63]==crc_reference((const char *)out+2,61));
    request[2]=0x2cU;reply_len=4U;
    assert(am13e_crsf_reply_envelope(request,6U,out,64U,workspace,1024U,execute,crc_reference)==7U);
    request[2]=0x2dU;
    assert(am13e_crsf_reply_envelope(request,6U,out,64U,workspace,1024U,execute,crc_reference)==7U);
    puts("PASS original CRSF cmd envelope, CRC, 61B clamp, preflight and types");
    return 0;
}
