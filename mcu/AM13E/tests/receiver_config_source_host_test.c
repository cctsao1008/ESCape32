/* Production startup validation -> io callback -> native decoder -> actual
 * util.c throttle/brake/CRC policy. No copied config or receiver policy. */
#include "common.h"
#include "io_backend.h"
#include <assert.h>
#include <stdio.h>
static unsigned feeds;
void am13e_app_io_watchdog_feed(void) { ++feeds; }
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
static void select_mode(unsigned mode)
{
    cfg.input_mode=(char)mode;cfg.input_ch1=1;cfg.input_ch2=2;
    cfg.throt_min=1000;cfg.throt_mid=1500;cfg.throt_max=2000;
    cfg.throt_mode=0;cfg.duty_drag=80;
    checkcfg();assert(am13e_app_io_input_mode()==mode);
    throt=123;brake=45;auxup=1;
}
static void packed(uint8_t *data,unsigned channel,unsigned value)
{
    for(unsigned b=0;b<11U;++b){
        const unsigned bit=channel*11U+b;
        if(value&(1U<<b))data[bit>>3U]|=(uint8_t)(1U<<(bit&7U));
    }
}
static void accepted(void)
{ assert(throt==2000 && brake==80 && !auxup); }
static void receiver_paths(void)
{
    uint8_t p[64]={0},reply[16];unsigned written=0;
    select_mode(2U);
    p[0]=5;p[1]=0;p[2]=0xd0;p[3]=7;p[4]=crc8((const char*)p,4);
    assert(am13e_app_io_serial_frame(p,5,reply,sizeof reply,&written));
    assert(throt==2000 && !written && feeds==1U);
    p[4]^=1U;assert(!am13e_app_io_serial_frame(p,5,reply,sizeof reply,&written));
    assert(feeds==1U && throt==2000);
    select_mode(3U);memset(p,0,sizeof p);p[0]=0x20;p[1]=0x40;
    for(unsigned i=1;i<=14U;++i){unsigned v=i==2U?1900U:2000U;p[i*2U]=(uint8_t)v;p[i*2U+1U]=(uint8_t)(v>>8U);}
    unsigned sum=0xffffU;for(unsigned i=0;i<30U;++i)sum-=p[i];
    p[30]=(uint8_t)sum;p[31]=(uint8_t)(sum>>8U);
    assert(am13e_app_io_ibus_frame(p,32));accepted();assert(feeds==2U);
    p[30]^=1U;assert(!am13e_app_io_ibus_frame(p,32));assert(feeds==2U);
    select_mode(4U);memset(p,0,sizeof p);p[0]=0x0f;
    packed(p+1U,0U,1792U);packed(p+1U,1U,1632U);p[24]=4U;
    telmode=0;telphid=1;temp1=25;temp2=30;curr=123;volt=456;csum=7;erpm=600;cfg.telem_poles=2;
    assert(am13e_app_io_sbus_frame(p,25));accepted();assert(feeds==3U);
    const uint8_t slots[]={0x43,0xc3,0x23,0xa3,0x63,0xe3};
    const uint8_t values[][2]={{125,0x80},{130,0x80},{0x40,123},{1,200},{0,7},{100,0}};
    for(unsigned n=0;n<6U;++n){
        uint8_t slot[3];unsigned delay=0;
        assert(am13e_app_io_sbus2_next_slot(slot,&delay));
        assert(slot[0]==slots[n] && slot[1]==values[n][0] && slot[2]==values[n][1]);
        assert(delay==(n?660U:3200U)); /* payload/delay requests only, not physical schedule */
    }
    unsigned delay;assert(!am13e_app_io_sbus2_next_slot(reply,&delay));
    assert(!am13e_app_io_sbus_frame(p,24));assert(feeds==3U);
    p[24]=0xff;assert(am13e_app_io_sbus_frame(p,25)); /* Original arbitrary end-byte admission. */
    assert(!am13e_app_io_sbus2_next_slot(reply,&delay));assert(feeds==4U);
    select_mode(5U);memset(p,0,sizeof p);p[0]=0xc8;p[1]=24;p[2]=0x16;
    packed(p+3U,0U,1792U);packed(p+3U,1U,1632U);
    p[25]=crc8dvbs2((const char*)p+2U,23);
    assert(am13e_app_io_crsf_frame(p,26));accepted();assert(feeds==5U);
    p[25]^=1U;assert(!am13e_app_io_crsf_frame(p,26));assert(feeds==5U);
#ifndef DISABLE_EXBUS
    select_mode(6U);memset(p,0,sizeof p);
    p[0]=0x3e;p[1]=1;p[2]=12;p[3]=0x22;p[4]=0x31;p[5]=4;
    p[6]=0x80;p[7]=0x3e; /* 2000*8 */
    p[8]=0x60;p[9]=0x3b; /* 1900*8 */
    uint16_t crc=crc16ccitt((const char*)p,10);p[10]=(uint8_t)crc;p[11]=(uint8_t)(crc>>8U);
    const unsigned before=feeds;
    assert(am13e_app_io_exbus_frame(p,12));accepted();assert(feeds==before+1U);
    p[10]^=1U;assert(!am13e_app_io_exbus_frame(p,12));assert(feeds==before+1U);
#endif
#ifndef DISABLE_HOTT
    select_mode(7U);memset(p,0,sizeof p);p[0]=0xa8;p[1]=1;p[2]=2;
    p[3]=0x3e;p[4]=0x80;p[5]=0x3b;p[6]=0x60;
    uint16_t sumd_crc=crc16xmodem((const char*)p,7);p[7]=(uint8_t)(sumd_crc>>8U);p[8]=(uint8_t)sumd_crc;
    const unsigned sumd_before=feeds;
    assert(am13e_app_io_hott_frame(p,9));accepted();assert(feeds==sumd_before+1U);
    p[8]^=1U;assert(!am13e_app_io_hott_frame(p,9));assert(feeds==sumd_before+1U);
#endif
    cfg.input_mode=0;const unsigned before_wrong=feeds;
    assert(!am13e_app_io_serial_frame(p,5,reply,sizeof reply,&written));
    assert(!am13e_app_io_ibus_frame(p,32) && !am13e_app_io_sbus_frame(p,25));
    assert(!am13e_app_io_crsf_frame(p,26) && !am13e_app_io_exbus_frame(p,12) && !am13e_app_io_hott_frame(p,9));
    assert(feeds==before_wrong);
}
#endif
int main(void)
{
    memcpy(&cfg,&cfgdata,sizeof cfg);
    for(unsigned mode=0U;mode<=7U;++mode){
        cfg.input_mode=(char)mode;cfg.input_ch1=32;cfg.input_ch2=32;checkcfg();
        unsigned expected=0U;
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
        if(mode>=2U)expected=mode;
#ifdef ANALOG_CHAN
        if(mode==1U)expected=1U;
#endif
#ifdef DISABLE_EXBUS
        if(mode==6U)expected=0U;
#endif
#ifdef DISABLE_HOTT
        if(mode==7U)expected=0U;
#endif
#elif defined(ANALOG_CHAN)
        if(mode==1U)expected=1U;
#endif
        assert(am13e_app_io_input_mode()==expected);
        assert(cfg.input_ch1==(expected>=3U?32:0));
        assert(cfg.input_ch2==(expected>=3U?32:0));
    }
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
    receiver_paths();
#endif
#ifdef AM13E_BOARD_SERIAL_RX_CONFIGURED
    puts("PASS production checkcfg capability selection and real serial/iBUS/SBUS/CRSF/EXBUS/SUMD callbacks");
#else
    puts("PASS production checkcfg refuses unselected receiver capabilities");
#endif
    return 0;
}
