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
    assert(!am13e_serial_frame_byte(&rx,0xffU,out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,&len));
    assert(rx.used==1U);
    assert(!am13e_serial_frame_byte(&rx,0x40U,out,&len));
    for(unsigned i=2;i<31;++i)
        assert(!am13e_serial_frame_byte(&rx,(uint8_t)i,out,&len));
    assert(am13e_serial_frame_byte(&rx,31U,out,&len));
    assert(len==32U&&out[0]==0x20U&&out[1]==0x40U&&out[31]==31U);
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,&len));
    am13e_serial_frame_abort(&rx);
    assert(rx.used==0U&&rx.frame_errors==1U);
    assert(!am13e_serial_frame_byte(&rx,0x40U,out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x20U,out,&len));
    assert(rx.used==1U);
}
static void sbus(void){
    AM13E_SerialFrameRx rx;
    uint8_t out[32]={0};unsigned len=0;
    am13e_serial_frame_init(&rx,4U);
    assert(!am13e_serial_frame_byte(&rx,0x55U,out,&len));
    assert(!am13e_serial_frame_byte(&rx,0x0fU,out,&len));
    for(unsigned i=1U;i<24U;++i)
        assert(!am13e_serial_frame_byte(&rx,(uint8_t)i,out,&len));
    assert(am13e_serial_frame_byte(&rx,0x04U,out,&len));
    assert(len==25U&&out[24]==0x04U&&rx.used==0U);
    assert(!am13e_serial_frame_byte(&rx,0x0fU,out,&len));
    am13e_serial_frame_abort(&rx);
    assert(!rx.used&&rx.frame_errors==1U);
    assert(!am13e_serial_frame_byte(NULL,0x0fU,out,&len));
    am13e_serial_frame_init(&rx,0U);
    assert(!am13e_serial_frame_byte(&rx,0x0fU,out,&len));
}
int main(void){ibus();sbus();puts("PASS native iBUS/SBUS UART FIFO framing, abort and resync");return 0;}
