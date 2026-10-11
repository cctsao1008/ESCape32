#include "serial_command.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t crc(const char *p,int n){
    uint8_t c=0;
    for(int i=0;i<n;++i){
        c^=(uint8_t)p[i];
        for(unsigned j=0;j<8U;++j)
            c=(uint8_t)((c<<1)^((c&0x80)?7:0));
    }
    return c;
}
static unsigned assemble(uint8_t *p,unsigned k){
    const unsigned n=k+2U;
    p[0]=(uint8_t)n;p[n-1U]=crc((const char *)p,(int)n-1);
    return n;
}
int main(void){
    AM13E_SerialCommandState s={.duty_drag=60,.temp1=30,.temp2=-1,
       .volt=4850,.curr=2250,.csum=1234,.ertm=18000,.led_count=0};
    uint8_t p[255]={0},reply[12]={0};unsigned got=0;
    p[1]=0x00;p[2]=0x30;p[3]=0xf8; /* -2000 */
    assert(am13e_serial_command(p,assemble(p,3),&s,reply,12,&got,crc));
    assert(s.throttle==-2000&&got==0);
    p[1]=0x01;p[2]=100U;
    assert(am13e_serial_command(p,assemble(p,2),&s,reply,12,&got,crc));
    assert(s.brake==(100*60*41>>12)&&s.auxup==0);
    p[1]=0x03U;
    assert(am13e_serial_command(p,assemble(p,1),&s,reply,12,&got,crc));
    assert(s.flipdir==1);
    p[1]=0x02U;
    assert(am13e_serial_command(p,assemble(p,1),&s,reply,12,&got,crc));
    assert(s.flipdir==0);
    for(unsigned c=0x80;c<=0x85;++c){
        p[1]=(uint8_t)c;
        assert(am13e_serial_command(p,assemble(p,1),&s,reply,12,&got,crc));
        assert(got==(c==0x80?12U:4U));
        assert(crc((const char *)reply,(int)got)==0);
    }
    p[1]=0x80;
    unsigned n=assemble(p,1);
    assert(!am13e_serial_command(p,n,&s,reply,11,&got,crc));
    p[n-1]^=1;
    assert(!am13e_serial_command(p,n,&s,reply,12,&got,crc));
    p[n-1]^=1;
    assert(!am13e_serial_command(p,n,&s,NULL,12,&got,crc));
    /* Full accepted command length range up to 255, not 64B-only. */
    for(unsigned i=1;i<254U;++i)p[i]=0x02U;
    n=assemble(p,254U);
    assert(n==255U);
    assert(am13e_serial_command(p,n,&s,reply,12,&got,crc));
    puts("PASS original Serial input_mode=2 throttle/brake/direction, CRC and six replies");
    return 0;
}
