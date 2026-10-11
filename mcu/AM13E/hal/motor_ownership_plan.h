/* AM13E MCPWM0 resource ownership. No physical gate authorization.
 * Pure transition policy used by the actual Rel17 motor/audio runtime.
 */
#pragma once
typedef enum {
    AM13E_OWNER_IDLE=0,
    AM13E_OWNER_SIXSTEP=1,
    AM13E_OWNER_SINE=2,
    AM13E_OWNER_BRAKE=3,
    AM13E_OWNER_MUSIC=4,
    AM13E_OWNER_PCM=5
} AM13E_MotorOwner;
typedef enum {
    AM13E_OWNER_EVENT_SIXSTEP,
    AM13E_OWNER_EVENT_SINE,
    AM13E_OWNER_EVENT_BRAKE,
    AM13E_OWNER_EVENT_MOTOR_START,
    AM13E_OWNER_EVENT_MOTOR_STOP,
    AM13E_OWNER_EVENT_MUSIC_START,
    AM13E_OWNER_EVENT_PCM_START,
    AM13E_OWNER_EVENT_AUDIO_END
} AM13E_OwnerEvent;
/* Returns 0 for forbidden/invalid ownership transition without writing
 * output. Running motor counter is tracked separately by the MCU timer.
 */
int am13e_motor_owner_next(AM13E_MotorOwner current,
                           AM13E_OwnerEvent action,
                           AM13E_MotorOwner *next);
