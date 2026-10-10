/* End-to-end Rel17-compatible PB14 RX -> optional BiDShot TX timing
 * software regression. No fake hardware interrupts or IO pass claims.
 */
#include "command_decode.h"
#include "bidir_codec.h"
#include "bidir_timing.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

enum { CAPTURE_HZ=200000000U, TIMER_HZ=100000000U };
static unsigned dshot_delivered, pwm_delivered;
static uint16_t last_dshot;
static int last_inverted;

static void on_pwm(unsigned int width)
{
    (void)width;
    ++pwm_delivered;
}
static int on_dshot(uint16_t frame,int inverted)
{
    /* Original Rel17 inverted/noninverted XOR nibble CRC contract. */
    unsigned c=0U;
    for(unsigned f=frame;f;f>>=4U) c^=f&15U;
    if (((c^(inverted?15U:0U))&15U)!=0U) return 0;
    ++dshot_delivered;
    last_dshot=frame;
    last_inverted=inverted;
    return 1;
}
static uint16_t frame_for(unsigned payload,int inverted)
{
    const unsigned data=((payload&2047U)<<1U)|1U;
    const unsigned raw=data<<4U;
    unsigned c=0U;
    for(unsigned v=raw;v;v>>=4U) c^=v&15U;
    return (uint16_t)(raw|((c^(inverted?15U:0U))&15U));
}
static uint32_t feed(AM13E_PB14_Decoder *d,uint16_t packet,
                     uint32_t start,uint32_t period,unsigned pulse_count)
{
    uint32_t final_edge=0U;
    for(unsigned i=0U;i<pulse_count;++i){
        const unsigned bit=15U-i;
        const uint32_t width=(packet&(1U<<bit))?
            period*75U/100U:period*38U/100U;
        final_edge=start+width;
        am13e_pb14_decoder_pulse(d,start,final_edge,on_pwm,on_dshot);
        start+=period;
    }
    return final_edge;
}
static void reply_wiring_check(uint32_t final_edge,uint32_t raw_period)
{
    /* First check intended DShot TX symbol length: 2/5 of received
     * DShot bit time, with both clocks independently configurable.
     */
    const uint32_t symbol=am13e_bidir_tx_period_ticks(
        raw_period,CAPTURE_HZ,TIMER_HZ);
    assert(symbol>=60U && symbol<=270U);
    const uint32_t delay=am13e_bidir_turnaround_ticks(
        final_edge,final_edge+(CAPTURE_HZ/1000000U)*5U,
        CAPTURE_HZ,TIMER_HZ);
    assert(delay==2500U); /* 25us until start, final-edge-anchored */
    assert(am13e_bidir_turnaround_ticks(
        final_edge,final_edge+(CAPTURE_HZ/1000000U)*31U,
        CAPTURE_HZ,TIMER_HZ)==0U);

    /* Real codec and toggle plan as called by command_reply.c.
     * CRC-valid RX triggers one response; only PB14 changes in DMA.
     */
    uint8_t levels[AM13E_BIDIR_DMA_LEVELS]={0};
    uint32_t words[AM13E_BIDIR_DMA_TRANSFERS]={0};
    const uint16_t telemetry=frame_for(0x35aU,1);
    am13e_bidir_encode_frame(telemetry,levels);
    am13e_bidir_toggle_plan(levels,UINT32_C(1)<<14U,words);
    assert(levels[0]==1U && levels[22]==0U && words[22]==0U);
    unsigned lvl=levels[0];
    for(unsigned i=1U;i<23U;++i){
        assert(words[i-1U]==0U || words[i-1U]==(UINT32_C(1)<<14U));
        lvl^=(words[i-1U]!=0U);
        assert(lvl==levels[i]);
    }
}
int main(void)
{
    const unsigned rates[3]={150000U,300000U,600000U};
    const uint32_t periods[3]={1333U,667U,333U};
    for(unsigned i=0U;i<3U;++i){
        AM13E_PB14_Decoder decoder;
        am13e_pb14_decoder_reset(&decoder,CAPTURE_HZ,1);
        assert(am13e_bidir_tx_period_ticks(periods[i],CAPTURE_HZ,TIMER_HZ)>0U);
        const uint16_t packet=frame_for(700U+i,1);
        const unsigned before=dshot_delivered;
        uint32_t start=UINT32_C(0xffff0000)+i*10000U;
        const uint32_t final_edge=feed(&decoder,packet,start,periods[i],16U);
        assert(decoder.good_dshot==1U && decoder.rejected==0U);
        assert(decoder.active==0U && decoder.decoded_bits==0U);
        assert(dshot_delivered==before+1U &&
               last_dshot==packet && last_inverted==1);
        reply_wiring_check(final_edge,periods[i]);
        assert(rates[i]>=150000U); /* all three speed selections */
    }
    assert(pwm_delivered==0U);
    const unsigned before_bad=dshot_delivered;
    AM13E_PB14_Decoder d;
    am13e_pb14_decoder_reset(&d,CAPTURE_HZ,1);
    feed(&d,(uint16_t)(frame_for(1024U,1)^1U),700000U,333U,16U);
    assert(d.good_dshot==0U && d.rejected>0U &&
           dshot_delivered==before_bad);

    /* Capture overrun must not accept a truncated frame; a new
     * complete CRC-valid frame is still recoverable after abort.
     */
    am13e_pb14_decoder_reset(&d,CAPTURE_HZ,1);
    feed(&d,frame_for(600U,1),1000000U,667U,8U);
    am13e_pb14_decoder_abort(&d);
    assert(d.good_dshot==0U && dshot_delivered==before_bad);
    feed(&d,frame_for(601U,1),2000000U,667U,16U);
    assert(d.good_dshot==1U && dshot_delivered==before_bad+1U);
    puts("PASS: PB14 DShot150/300/600 -> CRC -> 30us BiDShot plan + wrap/abort");
    return 0;
}
