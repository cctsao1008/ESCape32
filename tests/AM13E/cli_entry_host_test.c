#include "cli_entry.h"
#include <assert.h>
#include <stdio.h>
static int timeout(AM13E_CliEntry *s,int high)
{
    int ready=0;
    for(unsigned i=0;i<AM13E_CLI_ENTRY_TIMEOUT_TICKS;++i)
        ready=am13e_cli_entry_tick(s,high,0,0);
    return ready;
}
int main(void)
{
    AM13E_CliEntry s={0};
    for(unsigned n=0;n<100U;++n)assert(!timeout(&s,0));
    assert(s.high_timeouts==0U);
    for(unsigned n=0;n<15U;++n)assert(!timeout(&s,1));
    assert(s.high_timeouts==15U && !s.requested);
    assert(timeout(&s,1) && s.requested);
    assert(am13e_cli_entry_tick(&s,1,0,0));
    /* A byte/source-mode selection racing PendSV prevents entry. */
    am13e_cli_entry_select_receiver(&s);
    assert(!s.requested && !timeout(&s,1));
    s=(AM13E_CliEntry){0};
    for(unsigned n=0;n<500U;++n){
        for(unsigned i=0;i<AM13E_CLI_ENTRY_TIMEOUT_TICKS-1U;++i)
            assert(!am13e_cli_entry_tick(&s,1,0,0));
        assert(!am13e_cli_entry_tick(&s,1,1,0));
    }
    assert(s.high_timeouts==0U && s.ticks==0U);
    /* Source c is cumulative across LOW timeout observations. */
    for(unsigned n=0;n<15U;++n){assert(!timeout(&s,1));assert(!timeout(&s,0));}
    assert(timeout(&s,1));
    assert(!am13e_cli_entry_tick(&s,1,1,0) && !s.requested);
    assert(!am13e_cli_entry_tick(&s,1,0,1));
    for(unsigned n=0;n<20U;++n)assert(!timeout(&s,1));
    assert(!am13e_cli_entry_tick(NULL,1,0,0));
    am13e_cli_entry_select_receiver(NULL);
    puts("PASS original HIGH CLI timeout polarity/count, edge suppression and selected-receiver cancellation");
    return 0;
}
