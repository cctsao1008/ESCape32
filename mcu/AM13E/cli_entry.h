#pragma once
#include <stdint.h>
/* Original entryirq: sixteen HIGH timeout observations before CLI.
 * At 16kHz, ceil(65536us / 62.5us) = 1049 ticks per timeout.
 */
#define AM13E_CLI_ENTRY_TIMEOUT_TICKS 1049U
typedef struct {
    uint16_t ticks;
    uint8_t high_timeouts, selected, requested;
} AM13E_CliEntry;
int am13e_cli_entry_tick(AM13E_CliEntry *entry,int high,int edge,int selected);
void am13e_cli_entry_select_receiver(AM13E_CliEntry *entry);
