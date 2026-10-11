#include "cli_line_rx.h"
#include <stddef.h>
#include <string.h>
void am13e_cli_line_init(AM13E_CliLineRx *s){
    if(s)memset(s,0,sizeof *s);
}
void am13e_cli_line_abort(AM13E_CliLineRx *s){
    if(!s)return;
    s->used=0U;s->ready=false;s->reboot=false;
    s->discard_until_lf=true;++s->rejected;
}
int am13e_cli_line_byte(AM13E_CliLineRx *s,uint8_t b){
    if(!s)return -1;
    if(s->ready||s->reboot)return 0;
    if(s->discard_until_lf){
        if(b=='\n'){s->discard_until_lf=false;s->used=0U;}
        return -1;
    }
    if(b==8U||b==0x7fU){
        if(s->used!=0U)--s->used;
        return 0;
    }
    if(s->used==1U&&s->bytes[0]==0&&b==0xffU){
        s->reboot=true;return 2;
    }
    if(s->used>=AM13E_CLI_LINE_BYTES-1U){
        am13e_cli_line_abort(s);return -1;
    }
    s->bytes[s->used++]=(char)b;
    if(b!='\n')return 0;
    s->bytes[s->used]='\0';s->ready=true;
    return 1;
}
bool am13e_cli_line_take(AM13E_CliLineRx *s,char *out,unsigned capacity){
    if(!s||!out||!s->ready||s->reboot||
       capacity<AM13E_CLI_LINE_BYTES)return false;
    memcpy(out,s->bytes,s->used+1U);
    memset(s->bytes,0,sizeof s->bytes);
    s->used=0U;s->ready=false;
    return true;
}
