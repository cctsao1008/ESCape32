/* Execute production reset/quiesce functions; fake only their CMSIS and
 * physical safe-off/line-release boundaries. No physical qualification. */
#include "service_reset.h"
#include "handoff.h"
#include <soc.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

TestSysTick test_systick;
TestNVIC test_nvic;
TestSCB test_scb;
static jmp_buf reset_return;
static unsigned events[8], count;
enum { SAFE_OFF = 1, DSB, RESET, RELEASE, ISB };
static void event(unsigned value) { assert(count < 8U); events[count++] = value; }
void am13e_app_motor_fault_shutdown(void) { event(SAFE_OFF); }
void test_dsb(void) { event(DSB); }
void test_isb(void) { event(ISB); }
void test_reset(void) { event(RESET); longjmp(reset_return, 1); }
void boot_am13e_release_io(void)
{
    /* Release precedes disabling the timing source and clearing IRQs. */
    assert(test_systick.CTRL == 7U);
    event(RELEASE);
}
int main(void)
{
    if (!setjmp(reset_return)) am13e_app_service_reset();
    assert(count == 3U && events[0] == SAFE_OFF &&
           events[1] == DSB && events[2] == RESET);
    for (unsigned retry = 0U; retry < 2U; ++retry) {
        count = 0U;
        test_systick = (TestSysTick){7U, 0xffffffU, 123U};
        memset(&test_nvic, 0, sizeof test_nvic);
        test_scb.ICSR = UINT32_MAX;
        boot_am13e_quiesce();
        assert(count == 3U && events[0] == RELEASE &&
               events[1] == DSB && events[2] == ISB);
        assert(test_systick.CTRL == 0U && test_systick.LOAD == 0U &&
               test_systick.VAL == 0U);
        assert(test_nvic.ICER[0] == UINT32_MAX &&
               test_nvic.ICPR[0] == UINT32_MAX);
        assert(test_nvic.ICER[1] == 0xffffU && test_nvic.ICPR[1] == 0xffffU);
        for (unsigned i = 2U; i < 16U; ++i)
            assert(test_nvic.ICER[i] == 0U && test_nvic.ICPR[i] == 0U);
        assert(test_scb.ICSR == (SCB_ICSR_PENDSTCLR_Msk |
                                 SCB_ICSR_PENDSVCLR_Msk));
    }
    puts("PASS production service shutdown/reset order and Boot quiescence");
    return 0;
}
