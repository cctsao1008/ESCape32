/* Rel17 Motor Sixstep/Sine/Braking/Music/PCM MCPWM0 ownership:
 * test the SAME admission and transition planner used by motor_safety.c.
 * All phase GPIOs and PB13 still remain physically inactive.
 */
#include "motor_ownership_plan.h"
#include "motor_phase_plan.h"
#include "motor_aq_plan.h"
#include "motor_audio_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static void step(AM13E_MotorOwner *s, AM13E_OwnerEvent ev,
                 AM13E_MotorOwner expect)
{
    AM13E_MotorOwner dest=(AM13E_MotorOwner)99;
    assert(am13e_motor_owner_next(*s,ev,&dest));
    assert(dest==expect);
    *s=dest;
}
static void denied(AM13E_MotorOwner s,AM13E_OwnerEvent ev)
{
    AM13E_MotorOwner out=(AM13E_MotorOwner)99;
    assert(!am13e_motor_owner_next(s,ev,&out));
    assert((int)out==99); /* No side effect on forbidden transition. */
}
int main(void)
{
    AM13E_MotorOwner owner=AM13E_OWNER_IDLE;
    AM13E_SixstepPlan phase;
    AM13E_MotorAQShadowPlan aq;
    /* Real Rel17 6-step planar PWM and full 3-phase drag brake. */
    assert(am13e_motor_plan_sixstep(36,10,5,1,0,&phase));
    assert(am13e_motor_aq_plan_sixstep(&phase,&aq));
    step(&owner,AM13E_OWNER_EVENT_SIXSTEP,AM13E_OWNER_SIXSTEP);
    step(&owner,AM13E_OWNER_EVENT_MOTOR_START,AM13E_OWNER_SIXSTEP);
    denied(owner,AM13E_OWNER_EVENT_MUSIC_START);
    denied(owner,AM13E_OWNER_EVENT_PCM_START);
    step(&owner,AM13E_OWNER_EVENT_SINE,AM13E_OWNER_SINE);
    step(&owner,AM13E_OWNER_EVENT_MOTOR_START,AM13E_OWNER_SINE);
    denied(owner,AM13E_OWNER_EVENT_AUDIO_END);
    step(&owner,AM13E_OWNER_EVENT_SIXSTEP,AM13E_OWNER_SIXSTEP);
    assert(am13e_motor_aq_plan_drag_brake(&aq));
    step(&owner,AM13E_OWNER_EVENT_BRAKE,AM13E_OWNER_BRAKE);
    step(&owner,AM13E_OWNER_EVENT_MOTOR_START,AM13E_OWNER_BRAKE);
    denied(owner,AM13E_OWNER_EVENT_MUSIC_START);
    step(&owner,AM13E_OWNER_EVENT_MOTOR_STOP,AM13E_OWNER_IDLE);

    AM13E_AudioTone tone;
    assert(am13e_audio_tone(0,0,50,&tone) && tone.compare>0);
    step(&owner,AM13E_OWNER_EVENT_MUSIC_START,AM13E_OWNER_MUSIC);
    denied(owner,AM13E_OWNER_EVENT_SINE);
    denied(owner,AM13E_OWNER_EVENT_SIXSTEP);
    denied(owner,AM13E_OWNER_EVENT_BRAKE);
    denied(owner,AM13E_OWNER_EVENT_MOTOR_START);
    denied(owner,AM13E_OWNER_EVENT_MOTOR_STOP);
    denied(owner,AM13E_OWNER_EVENT_PCM_START);
    step(&owner,AM13E_OWNER_EVENT_AUDIO_END,AM13E_OWNER_IDLE);

    AM13E_AudioPCM pcm;
    assert(am13e_audio_pcm(127,100,&pcm) && pcm.w==0);
    step(&owner,AM13E_OWNER_EVENT_PCM_START,AM13E_OWNER_PCM);
    denied(owner,AM13E_OWNER_EVENT_MUSIC_START);
    denied(owner,AM13E_OWNER_EVENT_SIXSTEP);
    denied(owner,AM13E_OWNER_EVENT_SINE);
    denied(owner,AM13E_OWNER_EVENT_BRAKE);
    denied(owner,AM13E_OWNER_EVENT_MOTOR_START);
    step(&owner,AM13E_OWNER_EVENT_AUDIO_END,AM13E_OWNER_IDLE);
    denied(owner,AM13E_OWNER_EVENT_AUDIO_END);
    denied((AM13E_MotorOwner)99,AM13E_OWNER_EVENT_MOTOR_STOP);
    denied(owner,(AM13E_OwnerEvent)99);
    assert(!am13e_motor_owner_next(owner,AM13E_OWNER_EVENT_PCM_START,NULL));
    /* Exhaust all admissible/forbidden transitions as a finite matrix.
     * Every result must be a valid MCPWM0 owner, never two owners.
     */
    unsigned valid=0,invalid=0;
    for(int st=0;st<=5;++st)
       for(int ev=0;ev<=7;++ev) {
           AM13E_MotorOwner next;
           if(am13e_motor_owner_next((AM13E_MotorOwner)st,
                                    (AM13E_OwnerEvent)ev,&next)){
              assert(next>=AM13E_OWNER_IDLE && next<=AM13E_OWNER_PCM);
              ++valid;
           }else ++invalid;
       }
    assert(valid>0 && invalid>0 && valid+invalid==48U);
    puts("PASS: Sixstep/Sine/Brake/Music/PCM MCPWM0 ownership + 48 cases");
    return 0;
}
