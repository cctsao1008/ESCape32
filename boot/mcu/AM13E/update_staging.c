/* Original recvdata()/CRC and per-frame ACK remain in boot/src.
 * This file owns exactly 16KiB SRAM_S Boot bytes, no Flash commands.
 */
#include "update_staging.h"
#include <string.h>
_Static_assert(AM13E_BOOT_STAGE_BYTES==16384U,"Rev1.4 Boot must be 16KiB");
_Static_assert(AM13E_BOOT_STAGE_BLOCKS==16U,"16 original 1KiB frames");
static uint8_t stage[AM13E_BOOT_STAGE_BYTES] __attribute__((aligned(16)));
static unsigned length_bytes,frames;
static bool active,complete;

void boot_am13e_stage_abort(void)
{
    /* recvdata() can write a corrupt frame before rejecting its CRC.
     * Ensure failed/completed sessions cannot retain stale update data.
     */
    memset(stage,0,sizeof stage);
    length_bytes=0U;
    frames=0U;
    active=false;
    complete=false;
}
void boot_am13e_stage_begin(void)
{
    boot_am13e_stage_abort();
    active=true;
}
uint8_t *boot_am13e_stage_next(unsigned index)
{
    if(!active||complete||index>=AM13E_BOOT_STAGE_BLOCKS||
       index!=frames||length_bytes!=index*AM13E_BOOT_STAGE_BLOCK_BYTES)
        return 0;
    return &stage[length_bytes];
}
bool boot_am13e_stage_accept(unsigned index,unsigned len)
{
    if(!boot_am13e_stage_next(index)||len<4U||
       len>AM13E_BOOT_STAGE_BLOCK_BYTES||(len&3U)!=0U||
       len>AM13E_BOOT_STAGE_BYTES-length_bytes)
        return false;
    length_bytes+=len;
    ++frames;
    if(len<AM13E_BOOT_STAGE_BLOCK_BYTES||frames==AM13E_BOOT_STAGE_BLOCKS)
        complete=true;
    return true;
}
unsigned boot_am13e_stage_length(void){return active?length_bytes:0U;}
bool boot_am13e_stage_complete(void){return active&&complete;}
static uint32_t u32le(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|
           ((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);
}
bool boot_am13e_stage_vector_plausible(void)
{
    if(!boot_am13e_stage_complete()||length_bytes<8U)return false;
    const uint32_t msp=u32le(stage),reset=u32le(stage+4);
    const uint32_t pc=reset&~UINT32_C(1);
    /* Basic reset-vector eligibility is NOT image-completeness proof.
     * RAM_S and Boot Flash bounds come from the selected native linker.
     */
    return msp>=UINT32_C(0x20000008)&&msp<=UINT32_C(0x20018000)&&
           (msp&7U)==0U&&(reset&1U)!=0U&&
           pc>=AM13E_FLASH_BOOT_BASE&&pc<AM13E_FLASH_BOOT_END&&
           pc<length_bytes;
}
