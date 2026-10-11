#include "cli_line_rx.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void){
    AM13E_CliLineRx s;char out[AM13E_CLI_LINE_BYTES];
    am13e_cli_line_init(&s);
    assert(!am13e_cli_line_take(&s,out,sizeof out));
    for(const char *p="helpX";*p;++p)
        assert(am13e_cli_line_byte(&s,(uint8_t)*p)==0);
    assert(am13e_cli_line_byte(&s,8U)==0);
    assert(am13e_cli_line_byte(&s,'\n')==1);
    assert(am13e_cli_line_byte(&s,'X')==0); /* pending line immutable */
    assert(!am13e_cli_line_take(&s,out,sizeof out-1U));
    assert(am13e_cli_line_take(&s,out,sizeof out));
    assert(strcmp(out,"help\n")==0);
    for(unsigned i=0;i<AM13E_CLI_LINE_BYTES-1U;++i)
        assert(am13e_cli_line_byte(&s,'A')==0);
    assert(am13e_cli_line_byte(&s,'X')==-1);
    assert(s.discard_until_lf&&s.rejected==1U);
    for(const char *p="save";*p;++p)
        assert(am13e_cli_line_byte(&s,(uint8_t)*p)==-1);
    assert(!am13e_cli_line_take(&s,out,sizeof out));
    assert(am13e_cli_line_byte(&s,'\n')==-1);
    assert(!s.discard_until_lf);
    assert(am13e_cli_line_byte(&s,'H')==0);
    am13e_cli_line_abort(&s); /* parity/framing error */
    assert(am13e_cli_line_byte(&s,'i')==-1);
    assert(am13e_cli_line_byte(&s,'\n')==-1);
    assert(s.rejected==2U);
    assert(am13e_cli_line_byte(&s,0U)==0);
    assert(am13e_cli_line_byte(&s,0xffU)==2);
    assert(s.reboot&&!s.ready);
    assert(am13e_cli_line_byte(&s,'\n')==0);
    assert(!am13e_cli_line_take(&s,out,sizeof out));
    am13e_cli_line_init(&s);
    assert(am13e_cli_line_byte(&s,0x7fU)==0);
    assert(am13e_cli_line_byte(&s,'\n')==1);
    assert(am13e_cli_line_take(&s,out,sizeof out));
    assert(strcmp(out,"\n")==0);
    assert(am13e_cli_line_byte(NULL,'X')==-1);
    am13e_cli_line_abort(NULL);am13e_cli_line_init(NULL);
    puts("PASS original CLI newline/editing, 1024-byte bound, overflow quarantine and deferred magic reboot");
    return 0;
}
