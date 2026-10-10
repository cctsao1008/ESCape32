/* AM13E reference PB14 independent host test: timestamps and Rel17 callback handoff. */
#include "command_decode.h"
#include <assert.h>
#include <stdio.h>

static unsigned seen_pwm, last_pwm, seen_dshot, last_inverse;
static uint16_t last_frame;
static void on_pwm(unsigned width_us) { ++seen_pwm; last_pwm = width_us; }
static int on_dshot(uint16_t frame, int inverted)
{
    /* Same XOR-nibble CRC relation used by original Rel17 callback. */
    unsigned v=frame, crc=0;
    while(v){ crc ^= v;v>>=4; }
    if (((crc ^ (inverted ? 15U:0U)) & 15U) != 0U) return 0;
    ++seen_dshot;last_frame=frame;last_inverse=(unsigned)inverted;
    return 1;
}
static uint16_t encode(unsigned value, int inverted)
{
    const unsigned data = (value & 0x7ffU) << 1U;
    const unsigned raw = data << 4U;
    unsigned c=0;
    for(unsigned t=raw;t;t>>=4)c ^= t;
    return (uint16_t)(raw | ((c ^ (inverted?15U:0U)) & 15U));
}
static void feed_frame(AM13E_PB14_Decoder *d,uint16_t frame,uint32_t start,
                       uint32_t period,int inverted)
{
    (void)inverted;
    for(int bit=15;bit>=0;--bit){
        const uint32_t width = ((frame>>bit)&1U)?period*75U/100U:period*38U/100U;
        am13e_pb14_decoder_pulse(d,start,start+width,on_pwm,on_dshot);
        start += period;
    }
    /* A complete DShot frame must already have reached Rel17 before
     * this gap: the BiDShot reply cannot await a 62.5us SysTick.
     */
    assert(d->decoded_bits == 0U && d->active == 0U);
    am13e_pb14_decoder_idle(d,start+period*8U,on_dshot);
}
int main(void)
{
    AM13E_PB14_Decoder d;
    am13e_pb14_decoder_reset(&d,200000000U,0);
    am13e_pb14_decoder_pulse(&d,1000000U,1200000U,on_pwm,on_dshot);
    am13e_pb14_decoder_pulse(&d,5000000U,5220000U,on_pwm,on_dshot);
    assert(seen_pwm==1 && last_pwm==1000U && d.good_pwm==1U);
    am13e_pb14_decoder_reset(&d,200000000U,0);
    const uint16_t valid=encode(512U,0);
    feed_frame(&d,valid,100000U,333U,0); /* DShot600 @ 200MHz */
    assert(seen_dshot==1 && last_frame==valid && last_inverse==0);
    assert(d.good_dshot==1U);
    am13e_pb14_decoder_reset(&d,200000000U,1);
    const uint16_t inv=encode(512U,1);
    feed_frame(&d,inv,800000U,667U,1); /* inverted DShot300 RX, no TX */
    assert(seen_dshot==2 && last_frame==inv && last_inverse==1);
    am13e_pb14_decoder_reset(&d,200000000U,0);
    feed_frame(&d,(uint16_t)(valid^1U),1400000U,1333U,0); /* bad CRC */
    assert(seen_dshot==2 && d.good_dshot==0U && d.rejected>0U);
    /* Simulate capture phase advancing during a four-event read:
     * an aborted partial packet must not reach the Rel17 callback.
     * Subsequent complete, CRC-valid packets must still be accepted.
     */
    am13e_pb14_decoder_reset(&d,200000000U,0);
    am13e_pb14_decoder_pulse(&d,2000000U,2000250U,on_pwm,on_dshot);
    am13e_pb14_decoder_pulse(&d,2000333U,2000483U,on_pwm,on_dshot);
    am13e_pb14_decoder_abort(&d);
    assert(d.rejected==1U && d.active==0U && d.decoded_bits==0U);
    assert(seen_dshot==2);
    feed_frame(&d,valid,3000000U,333U,0);
    assert(seen_dshot==3 && d.good_dshot==1U);
    /* Non-DSHOT accepted cases: servo width/period and 32-bit wrap. */
    assert(am13e_pb14_capture_group_valid(
        100000U, 100150U, 100333U, 100580U, 200000000U));
    assert(am13e_pb14_capture_group_valid(
        100000U, 300000U, 4100000U, 4360000U, 200000000U));
    assert(am13e_pb14_capture_group_valid(
        UINT32_C(0xfffffff0), 0x60U, 0x155U, 0x1f0U, 200000000U));

    /* Corrupt/non-monotonic timestamps must not feed throttle. */
    assert(!am13e_pb14_capture_group_valid(
        4000U, 3800U, 4500U, 4650U, 200000000U));
    assert(!am13e_pb14_capture_group_valid(
        4000U, 4000U, 4500U, 4650U, 200000000U));
    assert(!am13e_pb14_capture_group_valid(
        4000U, 4100U, 4099U, 4200U, 200000000U));
    assert(!am13e_pb14_capture_group_valid(
        4000U, 4100U, 4200U, 4250U, 0U));
    puts("PB14 PWM/DShot RX + capture-abort/group-validation host test PASS");
    return 0;
}
