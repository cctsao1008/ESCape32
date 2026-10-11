#include "cli_entry.h"
#include <stddef.h>
void am13e_cli_entry_select_receiver(AM13E_CliEntry *s)
{
    if(s==NULL)return;
    s->selected=1U;
    s->requested=0U;
}
int am13e_cli_entry_tick(AM13E_CliEntry *s,int high,int edge,int selected)
{
    if(s==NULL)return 0;
    if(selected)am13e_cli_entry_select_receiver(s);
    if(s->selected)return 0;
    if(edge){s->ticks=0U;s->requested=0U;return 0;}
    if(s->requested)return 1;
    if(++s->ticks<AM13E_CLI_ENTRY_TIMEOUT_TICKS)return 0;
    s->ticks=0U;
    /* Rel17 entryirq resets capture counter n on LOW, but does not
     * reset HIGH timeout counter c. Preserve this source detail.
     */
    if(!high)return 0;
    if(++s->high_timeouts<16U)return 0;
    s->requested=1U;
    return 1;
}
