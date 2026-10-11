/* Original Rel17 38400baud CLI line editing; not a command parser.
 * Board-verified physical CLI entry is required separately.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define AM13E_CLI_LINE_BYTES 1024U
typedef struct {
    char bytes[AM13E_CLI_LINE_BYTES];
    unsigned used;
    unsigned rejected;
    bool ready;
    bool reboot;
    bool discard_until_lf;
} AM13E_CliLineRx;
void am13e_cli_line_init(AM13E_CliLineRx *s);
void am13e_cli_line_abort(AM13E_CliLineRx *s);
/* 0=collect,1=newline,2=original 00ff magic,-1=discard/overflow */
int am13e_cli_line_byte(AM13E_CliLineRx *s,uint8_t b);
bool am13e_cli_line_take(AM13E_CliLineRx *s,char *out,unsigned capacity);
