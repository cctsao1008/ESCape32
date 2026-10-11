/* Actual WWDT adapter, mocked TI register operations. No timeout or
 * silicon-reset qualification is inferred from these call assertions. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../input_watchdog.c"
uint32_t test_primask=1U;
static unsigned powered,running,starts,restarts;
static int behavior;
static jmp_buf fault;
void hard_fault_handler(void){longjmp(fault,1);}
void test_halt(void){assert(0);}
void DL_WWDT_enablePower(void *w){assert(w==WWDT0);powered=1U;}
bool DL_WWDT_isPowerEnabled(void *w){assert(w==WWDT0);return powered!=0U;}
bool DL_WWDT_isRunning(void *w){assert(w==WWDT0);return running!=0U;}
void DL_SYSCTL_setWWDT0ErrorBehavior(int b){behavior=b;}
int DL_SYSCTL_getWWDT0ErrorBehavior(void){return behavior;}
void DL_WWDT_initWatchdogMode(void *w,int divide,int bits,int sleep,int w0,int w1)
{
    assert(w==WWDT0 && powered && !running && divide==1 && bits==15 &&
           sleep==1 && w0==0 && w1==0);
    ++starts;running=1U;
}
void DL_WWDT_restart(void *w){assert(w==WWDT0 && running);++restarts;}
int main(void)
{
    if(!setjmp(fault)){am13e_app_io_watchdog_arm();assert(0);}
    assert(!starts && !restarts && !powered);
    am13e_app_io_watchdog_prepare();assert(powered && !running && !starts);
    test_primask=0U;am13e_app_io_watchdog_arm();
    assert(starts==1U && !restarts && !valid_frame_feeds);
    for(unsigned n=0;n<100U;++n)am13e_app_io_watchdog_arm();
    assert(starts==1U && !restarts); /* Invalid traffic cannot extend expiry. */
    am13e_app_io_watchdog_feed();assert(restarts==1U && valid_frame_feeds==1U);
    uint32_t a=0,f=0;am13e_app_io_watchdog_status(&a,&f);assert(a==1U && f==1U);
    test_primask=1U;
    if(!setjmp(fault)){am13e_app_io_watchdog_prepare();assert(0);}
    assert(starts==1U && restarts==1U);
    running=0U;
    if(!setjmp(fault)){am13e_app_io_watchdog_feed();assert(0);}
    assert(starts==1U && restarts==1U && valid_frame_feeds==1U);
    puts("PASS production WWDT selection arm, CRC-only restart and fail-closed ownership");
    return 0;
}
