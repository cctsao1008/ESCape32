/* Execute/link original default Cfg and globals from production main.c.
 * Uncalled hardware control loop is removed by section GC. */
#ifdef REL17_BEMF_POLICY_FIXTURE
#include "interfaces/motor_bemf_policy.h"
#endif
#include "../../main.c"

#ifdef REL17_BEMF_POLICY_FIXTURE
void am13e_app_motor_fault_shutdown(void) { abort(); }
void am13e_app_motor_fault_reset(void) { abort(); }
/* Expose the actual Core policy to existing host vectors, no copied formula. */
int am13e_bemf_policy_plan(int interval,int electrical,int captured,int timing,
                           AM13E_BemfPolicyPlan *out) {
    if(!out || !bemf_inputs_valid(interval,captured,timing))return -1;
    if(captured<(interval>>1))return 0;
    ival=interval;ertm=electrical;cfg.timing=(char)timing;
    out->delay_us=bemf_zero_cross_delay(captured);
    out->interval_us=ival;out->fast=fast;
    return 1;
}
#endif
