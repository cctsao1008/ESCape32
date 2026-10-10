/* Rel17 stationary lock (duty_lock=1/2) must reuse ORIGINAL six-step
 * phase masks. It must not synthesize a different "brake waveform".
 * cfg.throt_ztc can intentionally clear both masks to leave all gates off.
 *
 * Host:
 *   cc -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -Imcu/AM13E \
 *      mcu/AM13E/motor_phase_plan.c mcu/AM13E/motor_aq_plan.c \
 *      mcu/AM13E/tests/motor_lock_aq_host_test.c -o /tmp/lock_aq
 *   /tmp/lock_aq
 *
 * This validates the logic/AQ contract, NOT output non-overlap or driver
 * polarity on the physical E62 power stage.
 */
#include "motor_phase_plan.h"
#include "motor_aq_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static const uint16_t rel17_seq[6] = {
    UINT16_C(0x175),UINT16_C(0x0d9),UINT16_C(0x1ab),
    UINT16_C(0x072),UINT16_C(0x1de),UINT16_C(0x0ac)
};

int main(void)
{
    unsigned held=0U,coast=0U;
    for(unsigned lock=1U;lock<=2U;++lock)
    for(unsigned reverse=0U;reverse<2U;++reverse)
    for(unsigned step=0U;step<6U;++step)
    for(unsigned damp=0U;damp<2U;++damp) {
        /* Exact original src/main.c mask decoding. */
        const int x=(int)rel17_seq[step];
        const int m=x>>3;
        const int positive=x&m, negative=(~x)&m;
        const int cc=(m>>3)^((int)reverse<<2);
        AM13E_SixstepPlan plan;
        AM13E_MotorAQShadowPlan aq;
        assert(am13e_motor_plan_sixstep(positive,negative,cc,
                                          (int)damp,(int)reverse,&plan));
        assert(am13e_motor_aq_plan_sixstep(&plan,&aq));
        assert(am13e_motor_aq_plan_validate(&aq));
        unsigned high=0U,low=0U;
        for(unsigned i=0U;i<3U;++i) {
            if(aq.action[i*2U]) ++high;
            if(aq.action[i*2U+1U]==UINT16_C(0x0002)) ++low;
        }
        /* The only legal locked source image has one PWM and one
         * independent static return phase, not three simultaneously.
         */
        assert(high==1U && low==1U);
        ++held;
        /* Rel17 zero-throttle coast: p=n=0 suppresses lock drive.
         * Any comparator code may accompany this zero-drive request.
         */
        assert(am13e_motor_plan_sixstep(0,0,cc,(int)damp,
                                        (int)reverse,&plan));
        assert(am13e_motor_aq_plan_sixstep(&plan,&aq));
        assert(am13e_motor_aq_plan_validate(&aq));
        for(unsigned i=0U;i<6U;++i)assert(aq.action[i]==0U);
        ++coast;
    }
    assert(held==48U && coast==48U);
    printf("PASS: Rel17 lock=1/2 48 AQ hold images and 48 ZTC coast images\n");
    return 0;
}
