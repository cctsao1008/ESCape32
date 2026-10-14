/* Byte-stream resynchronization and per-transport receive framing. */
#include "serial_frame_rx.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void ibus(void){
    AM13E_SerialFrameRx rx;
    uint8_t out[32]={0};unsigned len=0;
    am13e_serial_frame_init(&rx,3U);
    assert(!am13e_serial_frame_byte(&rx,0xffU,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,sizeof out,&len));
    assert(rx.used==1U);
    assert(!am13e_serial_frame_byte(&rx,0x40U,out,sizeof out,&len));
    for(unsigned i=2;i<31;++i)
        assert(!am13e_serial_frame_byte(&rx,(uint8_t)i,out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,31U,out,sizeof out,&len));
    assert(len==32U&&out[0]==0x20U&&out[1]==0x40U&&out[31]==31U);
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,sizeof out,&len));
    am13e_serial_frame_abort(&rx);
    assert(rx.used==0U&&rx.frame_errors==1U);
    assert(!am13e_serial_frame_byte(&rx,0x40U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,sizeof out,&len));
    assert(rx.used==1U);
}
static void sbus(void){
    AM13E_SerialFrameRx rx;
    uint8_t out[32]={0};unsigned len=0;
    am13e_serial_frame_init(&rx,4U);
    assert(!am13e_serial_frame_byte(&rx,0x55U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x0fU,out,sizeof out,&len));
    for(unsigned i=1U;i<24U;++i)
        assert(!am13e_serial_frame_byte(&rx,(uint8_t)i,out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,0x04U,out,sizeof out,&len));
    assert(len==25U&&out[24]==0x04U&&rx.used==0U);
    assert(!am13e_serial_frame_byte(&rx,0x0fU,out,sizeof out,&len));
    am13e_serial_frame_abort(&rx);
    assert(!rx.used&&rx.frame_errors==1U);
    assert(!am13e_serial_frame_byte(NULL,0x0fU,out,sizeof out,&len));
    am13e_serial_frame_init(&rx,0U);
    assert(!am13e_serial_frame_byte(&rx,0x0fU,out,sizeof out,&len));
}
static void crsf(void){
    AM13E_SerialFrameRx rx;
    uint8_t out[64]={0};unsigned len=0U;
    am13e_serial_frame_init(&rx,5U);
    assert(!am13e_serial_frame_byte(&rx,0xc8U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0U,out,sizeof out,&len));
    assert(rx.used==0U&&rx.frame_errors==1U);
    assert(!am13e_serial_frame_byte(&rx,0xc8U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,62U,out,sizeof out,&len));
    for(unsigned i=2U;i<63U;++i)
        assert(!am13e_serial_frame_byte(&rx,(uint8_t)i,out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,63U,out,sizeof out,&len));
    assert(len==64U&&out[0]==0xc8U&&out[1]==62U&&out[63]==63U);
    uint8_t short_out[32]={0};
    assert(!am13e_serial_frame_byte(&rx,0xc8U,short_out,sizeof short_out,&len));
    assert(!am13e_serial_frame_byte(&rx,62U,short_out,sizeof short_out,&len));
    for(unsigned i=2U;i<64U;++i)
        assert(!am13e_serial_frame_byte(&rx,(uint8_t)i,
                                        short_out,sizeof short_out,&len));
    assert(rx.used==0U&&rx.frame_errors==2U);
    assert(!am13e_serial_frame_byte(&rx,0xc8U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,2U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x16U,out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,0U,out,sizeof out,&len));
    assert(len==4U);
    am13e_serial_frame_abort(&rx);
    assert(rx.used==0U&&rx.expected==0U);
}
static void exbus(void){
    AM13E_SerialFrameRx rx;uint8_t out[64]={0};unsigned len=0;
    am13e_serial_frame_init(&rx,6U);
    const uint8_t packet[]={0x3eU,0x01U,12U,0x10U,0x31U,4U,
                            0x40U,0x1fU,0xe0U,0x2eU,0x12U,0x23U};
    for(unsigned i=0;i<11U;++i)
        assert(!am13e_serial_frame_byte(&rx,packet[i],out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,packet[11],out,sizeof out,&len));
    assert(len==12U&&memcmp(out,packet,12U)==0);
    assert(!am13e_serial_frame_byte(&rx,0x3eU,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,1U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,73U,out,sizeof out,&len));
    assert(rx.used==0U&&rx.frame_errors==1U);
    /* A valid 8-byte telemetry request can be emitted without
     * enabling any unqualified physical TX route.
     */
    const uint8_t request[]={0x3eU,1U,8U,0x44U,0x3aU,0U,0U,0U};
    for(unsigned i=0;i<7U;++i)
        assert(!am13e_serial_frame_byte(&rx,request[i],out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,request[7],out,sizeof out,&len));
    assert(len==8U);
}
static void hott(void){
    AM13E_SerialFrameRx rx;uint8_t out[64]={0};unsigned len=0;
    am13e_serial_frame_init(&rx,7U);
    assert(!am13e_serial_frame_byte(&rx,0U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,0xa8U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,1U,out,sizeof out,&len));
    assert(!am13e_serial_frame_byte(&rx,33U,out,sizeof out,&len));
    assert(rx.used==0U&&rx.frame_errors==1U);
    const uint8_t packet[]={0xa8U,1U,2U,0x20U,0x40U,
                            0x30U,0x50U,0x00U,0x00U};
    for(unsigned i=0;i<8U;++i)
        assert(!am13e_serial_frame_byte(&rx,packet[i],out,sizeof out,&len));
    assert(am13e_serial_frame_byte(&rx,packet[8],out,sizeof out,&len));
    assert(len==9U&&memcmp(out,packet,9U)==0);
    assert(!am13e_serial_frame_byte(&rx,0xa8U,out,sizeof out,&len));
    am13e_serial_frame_abort(&rx);
    assert(rx.used==0U);
}
/* Real 16kHz tick gaps reset incomplete packets in the same
 * framing code that the UNICOMM ISR invokes. Verify boundary
 * equality, counter wrap and no reset of completed frames.
 */
static void idle_boundaries(void){
    AM13E_SerialFrameRx rx;
    uint8_t out[64]={0};unsigned length=0U;
    am13e_serial_frame_init(&rx,6U);
    am13e_serial_frame_gap(&rx,100U,4U);
    assert(!am13e_serial_frame_byte(&rx,0x3eU,out,sizeof out,&length));
    assert(!am13e_serial_frame_byte(&rx,1U,out,sizeof out,&length));
    assert(rx.used==2U);
    am13e_serial_frame_gap(&rx,103U,4U);
    assert(rx.used==2U&&rx.frame_errors==0U);
    am13e_serial_frame_gap(&rx,107U,4U);
    assert(rx.used==0U&&rx.frame_errors==1U);
    assert(!am13e_serial_frame_byte(&rx,0x3eU,out,sizeof out,&length));
    assert(rx.used==1U);
    am13e_serial_frame_gap(&rx,111U,4U);
    assert(rx.used==0U&&rx.frame_errors==2U);
    am13e_serial_frame_gap(&rx,120U,4U);
    assert(rx.frame_errors==2U); /* complete/idle no spurious errors */
    am13e_serial_frame_init(&rx,7U);
    am13e_serial_frame_gap(&rx,UINT32_MAX-1U,4U);
    assert(!am13e_serial_frame_byte(&rx,0xa8U,out,sizeof out,&length));
    am13e_serial_frame_gap(&rx,1U,4U); /* elapsed three ticks */
    assert(rx.used==1U);
    am13e_serial_frame_gap(&rx,2U,4U); /* elapsed one tick */
    assert(rx.used==1U);
    am13e_serial_frame_gap(&rx,6U,4U);
    assert(rx.used==0U&&rx.frame_errors==1U);
    am13e_serial_frame_gap(NULL,6U,4U);
    am13e_serial_frame_gap(&rx,9U,0U);
}
int main(void){ibus();sbus();crsf();exbus();hott();idle_boundaries();
    puts("PASS iBUS/SBUS/CRSF/EXBUS/HoTT native UART framing, bounds, 16kHz idle and wrap");
    return 0;}
