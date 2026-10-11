/* Original Rel17 src/io.c::crsffunc() command response
 * after a fully CRC-validated 0x28/0x2c/0x2d request.
 *
 * Keep original execcrsfcmd() and crc8dvbs2() as call sites. This
 * adapter only forms 0xc8/len/response/crc packet, not a new parser.
 */
#include "crsf_command_reply.h"
#include <stddef.h>
#include <string.h>
unsigned am13e_crsf_reply_envelope(const uint8_t *frame,unsigned n,
                                   uint8_t *out,unsigned cap,
                                   char *scratch,unsigned scratch_bytes,
                                   AM13E_CrsfCommandExecutor exec,
                                   AM13E_CrsfCrc8 crc)
{
    if(!frame||!out||!scratch||!exec||!crc||
       cap<64U||scratch_bytes<1024U||n<4U||n>64U||
       frame[1]<2U||frame[1]>62U||
       (unsigned)frame[1]+2U!=n ||
       (frame[2]!=0x28U&&frame[2]!=0x2cU&&frame[2]!=0x2dU))
        return 0U;
    /* Same original args: type+payload, excluding trailing wire CRC.
     * Scratch is intentionally larger than max transmitted frame:
     * execcrsfcmd() constructs parameter strings before pagination.
     */
    int bytes=exec((const char *)frame+2,(int)frame[1]-1,scratch+2);
    if(bytes<=0)return 0U;
    if(bytes>61)bytes=61;
    const unsigned size=(unsigned)bytes+3U;
    if(size>cap)return 0U;
    scratch[0]=(char)0xc8;
    scratch[1]=(char)(bytes+1);
    scratch[bytes+2]=(char)crc(scratch+2,bytes);
    memcpy(out,scratch,size);
    return size;
}
