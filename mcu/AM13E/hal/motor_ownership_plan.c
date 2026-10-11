#include "motor_ownership_plan.h"
#include <stddef.h>
int am13e_motor_owner_next(AM13E_MotorOwner current,
                           AM13E_OwnerEvent action,
                           AM13E_MotorOwner *next)
{
    if(next==NULL || current>AM13E_OWNER_PCM ||
       action>AM13E_OWNER_EVENT_AUDIO_END) return 0;
    AM13E_MotorOwner output=current;
    const int audio=(current==AM13E_OWNER_MUSIC ||
                     current==AM13E_OWNER_PCM);
    switch(action) {
        case AM13E_OWNER_EVENT_SIXSTEP:
            if(audio)return 0;
            output=AM13E_OWNER_SIXSTEP;
            break;
        case AM13E_OWNER_EVENT_SINE:
            if(audio)return 0;
            output=AM13E_OWNER_SINE;
            break;
        case AM13E_OWNER_EVENT_BRAKE:
            if(audio)return 0;
            output=AM13E_OWNER_BRAKE;
            break;
        case AM13E_OWNER_EVENT_MOTOR_START:
            if(audio)return 0;
            if(current==AM13E_OWNER_IDLE)
                output=AM13E_OWNER_SIXSTEP;
            break;
        case AM13E_OWNER_EVENT_MOTOR_STOP:
            if(audio)return 0;
            output=AM13E_OWNER_IDLE;
            break;
        case AM13E_OWNER_EVENT_MUSIC_START:
            if(current!=AM13E_OWNER_IDLE)return 0;
            output=AM13E_OWNER_MUSIC;
            break;
        case AM13E_OWNER_EVENT_PCM_START:
            if(current!=AM13E_OWNER_IDLE)return 0;
            output=AM13E_OWNER_PCM;
            break;
        case AM13E_OWNER_EVENT_AUDIO_END:
            if(!audio)return 0;
            output=AM13E_OWNER_IDLE;
            break;
        default:return 0;
    }
    *next=output;
    return 1;
}
