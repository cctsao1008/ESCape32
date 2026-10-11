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
int main(void){ibus();sbus();crsf();
    puts("PASS iBUS/SBUS/CRSF native UART framing, explicit buffer bounds and abort");
    return 0;}
