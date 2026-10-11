/* Host adapters expose original private Core functions to existing fixtures.
 * All decoding, policy and packet construction execute production src/io.c;
 * this file only supplies state and mocked hardware/command boundaries. */
#include <stdbool.h>
#define REL17_IO_TEST_OBSERVER
#include "../../src/io.c"
#include "ibus_receiver.h"
#include "sbus_receiver.h"
#include "crsf_receiver.h"
#include "exbus_receiver.h"
#include "hott_receiver.h"
#include "sbus2_reply.h"
#include "serial_command.h"
#include "crsf_command_reply.h"
#include "exbus_telemetry_reply.h"
static AM13E_CrsfCommandExecutor fixture_execute;
void am13e_app_io_watchdog_feed(void) {}
int execcrsfcmd(const char *p,int n,char *out) {
    return fixture_execute ? fixture_execute(p,n,out) : 0;
}
static bool fixture_frame(const uint8_t *p,unsigned n,unsigned mode,
                           unsigned ch1,unsigned ch2,int *a,int *b,bool *channels) {
    if(!p || !a || !b || !channels)return false;
    cfg.input_mode=(char)mode;cfg.input_ch1=(char)ch1;cfg.input_ch2=(char)ch2;
    io_raw_throttle=io_raw_brake=-1;io_raw_channels=false;
    int valid=0;
    switch(mode) {
      case 3: valid=am13e_app_io_ibus_frame(p,n);break;
      case 4: valid=am13e_app_io_sbus_frame(p,n);break;
      case 5: valid=am13e_app_io_crsf_frame(p,n);break;
      case 6: valid=am13e_app_io_exbus_frame(p,n);break;
      case 7: valid=am13e_app_io_hott_frame(p,n);break;
    }
    *a=io_raw_throttle;*b=io_raw_brake;*channels=io_raw_channels;
    return !!valid;
}
bool am13e_ibus_decode_channels(const uint8_t *p,unsigned n,unsigned c1,
                                  unsigned c2,int *a,int *b) {
    bool channels;return fixture_frame(p,n,3,c1,c2,a,b,&channels);
}
bool am13e_sbus_decode_channels(const uint8_t *p,unsigned n,unsigned c1,
                                  unsigned c2,int *a,int *b) {
    bool channels;return fixture_frame(p,n,4,c1,c2,a,b,&channels);
}
bool am13e_crsf_decode_frame(const uint8_t *p,unsigned n,unsigned c1,
                                  unsigned c2,int *a,int *b,bool *channels) {
    return fixture_frame(p,n,5,c1,c2,a,b,channels);
}
bool am13e_exbus_decode_frame(const uint8_t *p,unsigned n,unsigned c1,unsigned c2,
                AM13E_ExbusCrc16 crc,int *a,int *b,bool *channels,bool *telem) {
    if(!crc || !telem)return false;
    bool valid=fixture_frame(p,n,6,c1,c2,a,b,channels);
    *telem=valid && p[4]==0x3a && p[5]==0;
    return valid;
}
bool am13e_hott_decode_frame(const uint8_t *p,unsigned n,unsigned c1,unsigned c2,
                         AM13E_HottCrc16 crc,int *a,int *b,bool *channels) {
    return crc && fixture_frame(p,n,7,c1,c2,a,b,channels);
}
bool am13e_sbus2_eligible(const uint8_t *p,unsigned n,unsigned mode,unsigned id) {
    cfg.input_mode=4;telmode=(char)mode;telphid=(char)id;
    return am13e_app_io_sbus_frame(p,n) && sbus2_reply_remaining;
}
bool am13e_sbus2_next_packet(AM13E_Sbus2ReplyState *s,
                  const AM13E_Sbus2ReplyValues *v,uint8_t out[3],unsigned *delay) {
    if(!s || !v || s->slot_index>=6 || v->telphid<1 || v->telphid>4 ||
       v->telem_poles<=0)return false;
    cfg.input_mode=4;cfg.telem_poles=(char)v->telem_poles;
    temp1=v->temp1;temp2=v->temp2;curr=v->curr;volt=v->volt;csum=v->csum;
    erpm=v->erpm;telphid=(char)v->telphid;
    sbus2_reply_remaining=6-s->slot_index;sbus2_reply_index=s->slot_index;
    if(!am13e_app_io_sbus2_next_slot(out,delay))return false;
    s->slot_index=(uint8_t)sbus2_reply_index;return true;
}
bool am13e_serial_command(const uint8_t *p,unsigned n,AM13E_SerialCommandState *s,
                 uint8_t *out,unsigned cap,unsigned *written,AM13E_SerialCrc8 crc) {
    if(!s || !crc)return false;
    cfg.input_mode=2;cfg.duty_drag=(char)s->duty_drag;cfg.led=(char)s->led;
    throt=s->throttle;brake=s->brake;auxup=s->auxup;flipdir=(char)s->flipdir;
    temp1=s->temp1;temp2=s->temp2;volt=s->volt;curr=s->curr;csum=s->csum;ertm=s->ertm;
    const bool ok=am13e_app_io_serial_frame(p,n,out,cap,written);
    s->throttle=throt;s->brake=brake;s->auxup=auxup;s->flipdir=flipdir;s->led=cfg.led;
    return ok;
}
unsigned am13e_crsf_reply_envelope(const uint8_t *p,unsigned n,uint8_t *out,
                unsigned cap,char *scratch,unsigned bytes,
                AM13E_CrsfCommandExecutor exec,AM13E_CrsfCrc8 crc) {
    if(!scratch || bytes<1024 || !exec || !crc)return 0;
    cfg.input_mode=5;telmode=4;fixture_execute=exec;
    const unsigned written=am13e_app_io_crsf_reply(p,n,out,cap);
    fixture_execute=0;return written;
}
unsigned am13e_exbus_telemetry_build(AM13E_ExbusTelemetryState *s,
                const AM13E_ExbusTelemetryValues *v,uint8_t id,uint8_t *out,
                unsigned cap,AM13E_ExbusCrc8 c8,AM13E_ExbusCrc16 c16) {
    if(!s || !v || !out || !c8 || !c16 || v->telem_poles<2)return 0;
    cfg.input_mode=6;cfg.telem_poles=(char)v->telem_poles;
    temp1=v->temp1;temp2=v->temp2;volt=v->volt;curr=v->curr;csum=v->csum;erpm=v->erpm;
    exbus_description_index=s->description_index;
    uint8_t packet[64];
    const unsigned n=am13e_app_io_exbus_telemetry(id,v->device_id,packet,sizeof packet);
    if(n>cap)return 0;
    memcpy(out,packet,n);s->description_index=exbus_description_index;return n;
}
