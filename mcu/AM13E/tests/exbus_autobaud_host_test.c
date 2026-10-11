/* Test original src/io.c::exbusfunc() retry n=10 exact parity,
 * early channel lock, CRC-valid telemetry and exhaustion behavior.
 */
#include "exbus_autobaud.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
    AM13E_ExbusAutobaud s={0};
    uint32_t next=0U;
    am13e_exbus_autobaud_init(NULL);
    assert(!am13e_exbus_autobaud_bad(NULL,&next));
    assert(!am13e_exbus_autobaud_bad(&s,NULL));
    am13e_exbus_autobaud_init(&s);
    assert(s.attempts_remaining==10U&&s.baud==125000U);
    am13e_exbus_autobaud_valid(&s,false);
    assert(s.attempts_remaining==10U&&s.baud==125000U);
    for(unsigned fail=1U;fail<=10U;++fail){
        assert(am13e_exbus_autobaud_bad(&s,&next));
        assert(s.attempts_remaining==10U-fail);
        assert(next==((fail&1U)?250000U:125000U));
        assert(s.baud==next);
        am13e_exbus_autobaud_valid(&s,false);
    }
    assert(!s.channels_seen && s.attempts_remaining==0U);
    assert(!am13e_exbus_autobaud_bad(&s,&next));
    assert(s.baud==125000U); /* original 10th retry */
    am13e_exbus_autobaud_init(&s);
    assert(am13e_exbus_autobaud_bad(&s,&next)&&next==250000U);
    am13e_exbus_autobaud_valid(&s,true);
    assert(s.channels_seen&&s.attempts_remaining==0U);
    for(unsigned i=0U;i<1000U;++i){
        assert(!am13e_exbus_autobaud_bad(&s,&next));
        am13e_exbus_autobaud_valid(&s,false);
        assert(s.baud==250000U);
    }
    puts("PASS original Rel17 EXBUS n=10 autobaud parity, lock, idle-retry");
    return 0;
}
