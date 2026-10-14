/* Original execcrsfcmd() reply envelope; preserve original
 * <=61-byte CRSF reply command/parameter format, unchanged.
 * Workspace 1024 bytes matches original Rel17 iobuf budget.
 */
#pragma once
#include <stdint.h>
typedef int (*AM13E_CrsfCommandExecutor)(const char *,int,char *);
typedef uint8_t (*AM13E_CrsfCrc8)(const char *,int);
unsigned am13e_crsf_reply_envelope(const uint8_t *request,unsigned length,
                                   uint8_t *reply,unsigned capacity,
                                   char *workspace,unsigned work_capacity,
                                   AM13E_CrsfCommandExecutor execute,
                                   AM13E_CrsfCrc8 crc);
