#pragma once
/* Original Rel17 entryirq timer-domain qualification. Hardware adapters
 * supply 1MHz Servo or 8MHz Oneshot capture periods, not pulse widths. */
#define REL17_SERVO_ENTRY_CAPTURES 8U
#define REL17_DSHOT_ENTRY_CAPTURES 4U
static inline int rel17_receiver_servo_period(int ticks)
{
    return ticks > 2000;
}
static inline int rel17_receiver_dshot_rate(int ticks,int limit)
{
    if(limit<=0)return -1;
    int rate=2;
    while(ticks>=limit)ticks>>=1,--rate;
    return rate;
}
