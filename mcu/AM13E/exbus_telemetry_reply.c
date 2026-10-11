/* Source-grounded port of original Rel17 src/io.c::exbusresp().
 * This is a packet generator ONLY; no board-specific ID is fabricated,
 * no line turnaround or TX timings are asserted.
 */
#include "exbus_telemetry_reply.h"
#include <stddef.h>
#include <string.h>
static const char *const names[]={
    "ESCape32","Temp1","Temp2","Volt","Curr","Csum","RPM"
};
static const char *const units[]={
    "","C","C","V","A","mAh",""
};
unsigned am13e_exbus_telemetry_build(AM13E_ExbusTelemetryState *state,
                                    const AM13E_ExbusTelemetryValues *v,
                                    uint8_t request_id,uint8_t *out,
                                    unsigned cap,AM13E_ExbusCrc8 crc8,
                                    AM13E_ExbusCrc16 crc16)
{
    if(!state||!v||!out||!crc8||!crc16||
       v->telem_poles<2)return 0U;
    const unsigned n=state->description_index;
    if(n>7U)return 0U;
    const unsigned name_size=(n<7U)?
        (unsigned)strlen(names[n]):0U;
    const unsigned unit_size=(n<7U)?
        (unsigned)strlen(units[n]):0U;
    const unsigned size=(n<7U)?name_size+unit_size+18U:38U;
    if(cap<size)return 0U;
    /* Original exbusresp() starts with these exact header bytes. */
    out[0]=0x3bU;
    out[1]=0x01U;
    out[2]=(uint8_t)size;
    out[3]=request_id;
    out[4]=0x3aU;
    out[5]=(uint8_t)(size-8U);
    out[6]=0x0fU;
    out[8]=(uint8_t)(v->device_id&0x1fU);
    out[9]=0xa4U;
    out[10]=(uint8_t)(v->device_id>>8U);
    out[11]=(uint8_t)(v->device_id>>16U);
    out[12]=0x00U;
    if(n<7U){
        /* Legacy description packet's iobuf[7] and [13:14].
         * The CRC8 replaces the last stpcpy NUL byte.
         */
        out[7]=(uint8_t)(size-10U);
        out[13]=(uint8_t)n;
        out[14]=(uint8_t)((name_size<<3U)|unit_size);
        memcpy(out+15U,names[n],name_size);
        memcpy(out+15U+name_size,units[n],unit_size);
        out[size-3U]=crc8((const char *)out+7U,
                              (int)(name_size+unit_size+8U));
        ++state->description_index;
    } else {
        /* Legacy data packet length=0x26; keep exact value IDs
         * and byte ordering from exbusresp(), not a new schema.
         */
        out[7]=0x5cU;
        out[13]=0x11U;
        out[14]=(uint8_t)v->temp1;
        out[15]=(uint8_t)(v->temp1>>8);
        out[16]=0x21U;
        out[17]=(uint8_t)v->temp2;
        out[18]=(uint8_t)(v->temp2>>8);
        out[19]=0x34U;
        out[20]=(uint8_t)v->volt;
        out[21]=(uint8_t)(v->volt>>8);
        out[22]=(uint8_t)((v->volt>>16)|0x40);
        out[23]=0x44U;
        out[24]=(uint8_t)v->curr;
        out[25]=(uint8_t)(v->curr>>8);
        out[26]=(uint8_t)((v->curr>>16)|0x40);
        out[27]=0x54U;
        out[28]=(uint8_t)v->csum;
        out[29]=(uint8_t)(v->csum>>8);
        out[30]=(uint8_t)(v->csum>>16);
        out[31]=0x64U;
        const int rpm=v->erpm/(v->telem_poles>>1);
        out[32]=(uint8_t)rpm;
        out[33]=(uint8_t)(rpm>>8);
        out[34]=(uint8_t)(rpm>>16);
        out[35]=crc8((const char *)out+7U,28);
    }
    const uint16_t checksum=crc16((const char *)out,(int)(size-2U));
    out[size-2U]=(uint8_t)checksum;
    out[size-1U]=(uint8_t)(checksum>>8U);
    return size;
}
