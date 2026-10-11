#include "exbus_autobaud.h"
#include <stddef.h>
void am13e_exbus_autobaud_init(AM13E_ExbusAutobaud *s)
{
    if(!s)return;
    s->attempts_remaining=10U;  /* upstream: static char n=10 */
    s->channels_seen=false;
    s->baud=125000U;
}
void am13e_exbus_autobaud_valid(AM13E_ExbusAutobaud *s,
                               bool channel_frame)
{
    if(!s)return;
    if(channel_frame){
        s->attempts_remaining=0U; /* upstream: n=0 on 0x31 */
        s->channels_seen=true;
    }
    /* Valid telemetry/unknown packet does not decrement n.
     * Do not reinterpret this as a signal to reset autobaud.
     */
}
bool am13e_exbus_autobaud_bad(AM13E_ExbusAutobaud *s,
                              uint32_t *requested_baud)
{
    if(!s||!requested_baud||s->channels_seen||
       s->attempts_remaining==0U)return false;
    --s->attempts_remaining;
    s->baud=(s->attempts_remaining&1U)?250000U:125000U;
    *requested_baud=s->baud;
    return true;
}
