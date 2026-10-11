/* Validate source EXBUS telemetry reply (seven descriptor packets
 * then 38-byte sensor payload), CRC8/CRC16, identifier injection,
 * size guards and stable request sequence. No physical TX qualification.
 */
#include "exbus_telemetry_reply.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t c8(const char *p,int n)
{
    uint8_t c=0U;
    for(int i=0;i<n;++i){
        c^=(uint8_t)p[i];
        for(unsigned j=0;j<8U;++j)
            c=(uint8_t)((c<<1U)^((c&0x80U)?7U:0U));
    }
    return c;
}
static uint16_t c16(const char *p,int n)
{
    uint16_t c=0U;
    for(int i=0;i<n;++i){
        uint8_t v=(uint8_t)(c^(uint8_t)p[i]);
        v^=(uint8_t)(v<<4);
        c=(uint16_t)((c>>8)^((uint16_t)v<<8)^
                     ((uint16_t)v<<3)^(v>>4));
    }
    return c;
}
int main(void)
{
    AM13E_ExbusTelemetryState state={0};
    const AM13E_ExbusTelemetryValues v={
        .device_id=0x89abcdefU,.temp1=20,.temp2=-5,
        .volt=0x12345,.curr=0x56789,.csum=0x34567,
        .erpm=24000,.telem_poles=14
    };
    uint8_t p[64]={0};
    for(unsigned i=0U;i<7U;++i){
        assert(!am13e_exbus_telemetry_build(&state,&v,0x33U,
                 p,17U,c8,c16));
        assert(state.description_index==i);
        unsigned n=am13e_exbus_telemetry_build(&state,&v,0x33U,
                                                p,sizeof p,c8,c16);
        assert(n>=18U&&n<=64U&&p[2]==n&&p[3]==0x33U);
        assert(p[0]==0x3bU&&p[1]==1U&&p[4]==0x3aU);
        assert(p[8]==(0x89abcdefU&0x1fU)&&p[9]==0xa4U);
        assert(p[10]==0xcdU&&p[11]==0xabU);
        assert(p[13]==i);
        assert(c16((const char *)p,(int)n)==0U);
        assert(c8((const char *)p+7,(int)n-10)==0U);
        assert(state.description_index==i+1U);
    }
    for(unsigned j=0U;j<32U;++j){
        unsigned n=am13e_exbus_telemetry_build(&state,&v,0x55U,
                                                p,sizeof p,c8,c16);
        assert(n==38U&&p[2]==38U&&p[3]==0x55U);
        assert(p[5]==0x1eU&&p[7]==0x5cU&&p[13]==0x11U);
        assert(p[14]==20U&&p[15]==0U);
        assert(p[17]==(uint8_t)-5&&p[18]==0xffU);
        assert(p[20]==0x45U&&p[21]==0x23U&&p[22]==0x41U);
        assert(p[24]==0x89U&&p[25]==0x67U&&p[26]==0x45U);
        assert(p[28]==0x67U&&p[29]==0x45U&&p[30]==0x03U);
        assert(p[31]==0x64U);
        assert(p[32]==(24000U/7U &255U));
        assert(p[35]==c8((const char *)p+7,28));
        assert(c16((const char *)p,38)==0U);
        assert(state.description_index==7U);
    }
    assert(!am13e_exbus_telemetry_build(NULL,&v,1U,p,64U,c8,c16));
    assert(!am13e_exbus_telemetry_build(&state,NULL,1U,p,64U,c8,c16));
    assert(!am13e_exbus_telemetry_build(&state,&v,1U,NULL,64U,c8,c16));
    assert(!am13e_exbus_telemetry_build(&state,&v,1U,p,37U,c8,c16));
    AM13E_ExbusTelemetryValues bad=v;bad.telem_poles=1;
    assert(!am13e_exbus_telemetry_build(&state,&bad,1U,p,64U,c8,c16));
    assert(state.description_index==7U);
    puts("PASS Rel17 EXBUS 7 descriptions, CRC8/CRC16, 38B telemetry and board-owned UID");
    return 0;
}
