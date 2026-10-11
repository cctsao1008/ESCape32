/* Real io/prog/main defaults + util checkcfg/savecfg/resetcfg + Flash writer.
 * Only physical cfg-commit, Flash operations, watchdog, TX-busy and audio
 * boundaries are mocked. Including util.c exposes its actual busy guard. */
#include "newlib_host.h"
#include "../../util.c"
#include "io_backend.h"
#include "command_reply.h"
#include "command_decode.h"
#include "cfg_flash_writer.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#ifdef BEC_MAP
#include "../../../mcu/AM13E/hal/aux_gpio.c" /* Actual production peripheral unit, not a model. */
#include <setjmp.h>
GPIO_Regs test_gpio[3];
unsigned test_function[96],test_input[96],test_enable_calls,test_read_toggle;
static jmp_buf bec_failed;
void test_halt(void) { longjmp(bec_failed,1); }
void test_output_enable(GPIO_Regs *g,uint32_t mask)
{
    assert(g==GPIO1 && mask && !(mask&~3U));
    assert(g->latch==(unsigned)am13e_app_bec_selector());
    assert(test_function[32]==1U && test_function[33]==1U);
}
static void bec_regressions(void)
{
    for(int value=BEC_MIN;value<=BEC_MAX;++value){
        cfg.bec=value;checkcfg();
        assert(am13e_app_bec_selector()==value-BEC_MIN);
        bec_initialized=0U; /* Simulated MCU reset between startup choices. */
        memset(test_gpio,0,sizeof test_gpio);
        memset(test_function,0,sizeof test_function);
        am13e_app_aux_gpio_init();
        assert(test_gpio[1].output==3U && test_gpio[1].latch==(unsigned)(value-BEC_MIN));
    }
    const unsigned enabled=test_enable_calls;
    if(!setjmp(bec_failed)){am13e_app_aux_gpio_init();assert(0);}
    assert(test_enable_calls==enabled);
    bec_initialized=0U;cfg.bec=BEC_MAX+1;
    if(!setjmp(bec_failed)){am13e_app_aux_gpio_init();assert(0);}
    assert(test_enable_calls==enabled);
    puts("PASS production Cfg BEC selector -> native GPIO startup bits/order; invalid/reinit no output enable");
}
#endif

char _cfg[4096];
static unsigned feeds, commits, erases, programs, reads;
static unsigned in_capture, tx_busy, fail_program, corrupt_read;
static unsigned inject_save;
static uint16_t packet(unsigned value,unsigned telemetry)
{
    const unsigned body=(value<<1U)|telemetry;
    return (uint16_t)((body<<4U)|((body^(body>>4U)^(body>>8U))&15U));
}
static void receive(unsigned value,unsigned telemetry)
{
    in_capture=1U;
    assert(am13e_app_io_dshot_packet(packet(value,telemetry),0));
    in_capture=0U;
}
static void captured(AM13E_PB14_Decoder *d,uint16_t value,unsigned period,uint32_t start)
{
    in_capture=1U;
    for(unsigned i=0;i<16U;++i) {
        const unsigned width=(value&(1U<<(15U-i)))?period*3U/4U:period*3U/8U;
        am13e_pb14_decoder_pulse(d,start,start+width,NULL,am13e_app_io_dshot_packet);
        start+=period;
    }
    in_capture=0U;
}
static void discovery_regressions(void)
{
    const unsigned periods[]={667U,333U,167U};
    cfg.input_mode=0;cfg.throt_mode=0;ertm=0;
    for(unsigned r=0;r<3U;++r) {
        AM13E_PB14_Decoder d;am13e_pb14_decoder_reset(&d,200000000U,0);
        throt=0;const unsigned before=feeds;
        captured(&d,packet(512U,0U)^1U,periods[r],100000U);
        assert(d.rx_mode==AM13E_PB14_RX_DSHOT && feeds==before && throt==0);
        am13e_pb14_decoder_idle(&d,20000000U,am13e_app_io_dshot_packet);
        assert(d.rx_mode==AM13E_PB14_RX_DSHOT);
        captured(&d,packet(512U,0U),periods[r],21000000U);
        assert(feeds==before+1U && throt==465 && d.good_dshot==1U);
        const unsigned other=periods[(r+1U)%3U];
        captured(&d,packet(700U,0U),other,22000000U);
        assert(feeds==before+1U && throt==465 && d.good_dshot==1U);
    }
    AM13E_PB14_Decoder d;am13e_pb14_decoder_reset(&d,200000000U,0);
    const unsigned before=feeds;throt=0;
    captured(&d,packet(512U,0U),1333U,100000U);
    assert(d.rx_mode==AM13E_PB14_RX_UNDECIDED && feeds==before && throt==0);
    puts("PASS production capture discovery -> original DShot CRC/throttle, locked rate and unsupported-rate rejection");
}
static void command(unsigned value)
{
    receive(0U,0U); /* Original zero-throttle packet resets repetition. */
    for(unsigned n=0U;n<6U;++n)receive(value,1U);
}
void am13e_app_io_watchdog_feed(void) { ++feeds; }
int am13e_pb14_bidir_tx_busy(void) { return (int)tx_busy; }
static int erase(uint32_t addr,void *ctx)
{
    (void)ctx;assert(!in_capture && (addr==0x4000U || addr==0x4800U));
    ++erases;memset(_cfg+addr-0x4000U,0xff,2048U);return 1;
}
static int program(uint32_t addr,const uint32_t words[4],void *ctx)
{
    (void)ctx;assert(!in_capture && addr>=0x4000U && addr<=0x4ff0U && !(addr&15U));
    ++programs;if(fail_program==programs)return 0;
    memcpy(_cfg+addr-0x4000U,words,16U);return 1;
}
static int readback(uint32_t addr,uint8_t out[16],void *ctx)
{
    (void)ctx;++reads;memcpy(out,_cfg+addr-0x4000U,16U);
    if(corrupt_read)out[0]^=1U;
    return 1;
}
int am13e_app_cfg_commit(const void *destination,const void *source,unsigned bytes)
{
    assert(!in_capture && destination==_cfg && source==&cfg && bytes==sizeof(Cfg));
    ++commits;
    /* Interrupt a running save at the mock hardware boundary. Repeated
     * source Save command must not reserve another transaction. */
    if(inject_save){inject_save=0U;command(12U);}
    if(!memcmp(destination,source,bytes))return 1;
    AM13E_CfgFlashPlan plan;
    assert(am13e_cfg_flash_plan(0x4000U,0x20001000U,bytes,&plan));
    const AM13E_CfgFlashOps ops={erase,program,readback,NULL};
    return am13e_cfg_flash_execute(&plan,source,&ops);
}
/* Audio is not under test; no fake success for a requested playback. */
void am13e_app_commutation_reset(void) { assert(0); }
void am13e_app_audio_music_begin(void) { assert(0); }
void am13e_app_audio_music_note(int note,int octave,int volume)
{ (void)note;(void)octave;(void)volume;assert(0); }
void am13e_app_audio_music_pause(void) { assert(0); }
void am13e_app_audio_music_tick(void) { assert(0); }
void am13e_app_audio_end(void) { assert(0); }
char *itoa(int value,char *out,int radix)
{ assert(radix==10);sprintf(out,"%d",value);return out; }
static void cli(const char *request,const char *response)
{
    char buffer[2048];strcpy(buffer,request);
    const int len=execcmd(buffer);
    assert(len==(int)strlen(response) && !strcmp(buffer,response));
}
int main(void)
{
    /* Production linker symbols mirror the actual Cfg, with no copied
     * record layout or fixed sizeof constant in this test. */
    __asm__(".global _cfg_start\n.set _cfg_start,cfg\n"
            ".global _cfg_end\n.set _cfg_end,cfg+%c0\n" :: "i"(sizeof(Cfg)));
    memset(_cfg,0xff,sizeof(_cfg));
    memcpy(&cfg,&cfgdata,sizeof cfg);checkcfg();
    assert(cfg.id==0x32eaU && cfg.revision==17 && cfg.revpatch==3);
    const Cfg defaults=cfg;
#ifdef BEC_MAP
    bec_regressions();memcpy(&cfg,&defaults,sizeof cfg);
#endif
    cfg.revdir=1;
    receive(0U,0U);
    for(unsigned n=0U;n<5U;++n)receive(47U,1U);
    am13e_app_io_service_pending_save();assert(commits==0U && cfg.revdir==1);
    receive(47U,1U);
    assert(cfg.revdir==1 && commits==0U && erases==0U && programs==0U);
    tx_busy=1U;am13e_app_io_service_pending_save();assert(commits==0U);
    tx_busy=0U;am13e_app_io_service_pending_save();
    assert(commits==1U && beepval==1 && rearm && !memcmp(&cfg,&defaults,sizeof cfg));
    assert(!memcmp(_cfg,&cfg,sizeof cfg) && erases==1U && reads==programs);
    const unsigned before=erases;
    command(12U);am13e_app_io_service_pending_save();assert(erases==before); /* idempotent */
    cfg.revdir=1;command(47U);throt=100;
    am13e_app_io_service_pending_save();assert(cfg.revdir==1 && beepval==0);
    throt=0;am13e_app_io_service_pending_save();assert(cfg.revdir==1); /* cancelled */
    command(12U);ertm=100;am13e_app_io_service_pending_save();ertm=0;
    assert(erases==before);
    command(12U);inject_save=1U;am13e_app_io_service_pending_save();
    const unsigned count=commits;am13e_app_io_service_pending_save();assert(commits==count);
    const unsigned fed=feeds;
    assert(!am13e_app_io_dshot_packet(packet(47U,1U)^1U,0));
    assert(feeds==fed && commits==count);
    for(unsigned n=0;n<6U;++n)receive(47U,0U); /* no telemetry: no command */
    am13e_app_io_service_pending_save();assert(commits==count);
    ertm=1;command(47U);ertm=0;am13e_app_io_service_pending_save();assert(commits==count);
    /* Original sixth repetition, command changes and throttle mappings. */
    command(8U);assert(cfg.revdir==1);
    command(7U);assert(!cfg.revdir);
    command(10U);receive(2047U,0U);assert(cfg.throt_mode==1 && throt==2000);
    receive(48U,0U);assert(throt==-2);
    command(9U);receive(2047U,1U);assert(throt==2000 && telreq);
    throt=0;
    cli("set timing 99","timing: 31\nOK\n");
    cli("set no_such_param 1","ERROR\n");
    cli("set timing invalid","ERROR\n");
    ertm=1;cli("save","ERROR\n");cli("reset","ERROR\n");ertm=0;
    busy=1;cli("save","ERROR\n");cli("reset","ERROR\n");busy=0;
    assert(commits==count);
    cli("save","OK\n");
    Cfg persisted=cfg;
    memset(&cfg,0,sizeof cfg);memcpy(&cfg,_cfg,sizeof cfg);checkcfg();
    assert(!memcmp(&cfg,&persisted,sizeof cfg)); /* actual startup copy/check policy */
    cfg.timing=30;fail_program=programs+1U;cli("save","ERROR\n");
    fail_program=0U;corrupt_read=1U;cli("save","ERROR\n");corrupt_read=0U;
    cli("reset","OK\n");assert(rearm && !memcmp(&cfg,&defaults,sizeof cfg));
    /* CRSF uses the actual prog.c parser and same configuration hooks. */
    cfg.timing=29;telphid=1;
    char reply[1024];
    const char save[]={0x2d,0x90,0xea,48,1};
    int len=execcrsfcmd(save,sizeof save,reply);
    assert(len>6 && !strcmp(reply+14,"Done!"));
    const char reset_query[]={0x2d,0x90,0xea,49,1};
    len=execcrsfcmd(reset_query,sizeof reset_query,reply);
    assert(len>6 && cfg.timing==29 && reply[6]==0x0d);
    const char reset_confirm[]={0x2d,0x90,0xea,49,4};
    len=execcrsfcmd(reset_confirm,sizeof reset_confirm,reply);
    assert(len>6 && !memcmp(&cfg,&defaults,sizeof cfg));
    discovery_regressions();
    puts("PASS production DShot deferred Save/Reset, CLI/CRSF config guards and Flash writer failures");
    return 0;
}
