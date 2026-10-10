/*
 * E62 AM13E23019 — real Rel17 savecfg() Flash persistence backend.
 *
 * SW Architecture Baseline v1.6:
 *   FW1 config 0x00004000..0x00004fff (two 2KiB erase sectors)
 *   FW2 config 0x00005000..0x00005fff (NEVER touched here)
 *
 * The original Rel17 savecfg() requires ertm==0 && busy==0 before call.
 * This adapter enforces MCU-side safety and validates source/destination
 * again, snapshots settings into SRAM, then invokes the existing
 * failure-reporting, 16-byte ECC planner/writer and TI DL_Flash APIs.
 * There is NO fabricated Flash driver and NO fake-success return.
 *
 * Each TI flash command masks IRQs individually; the TI SDK marks
 * DL_FlashCTL_executeCommand() RAMFUNC for flash-busy command polling.
 * This preserves opportunities for the original valid-input WWDT
 * feeding between commands; it NEVER feeds WWDT unconditionally.
 * A Flash transaction is NOT power-loss atomic; the original FW1
 * configuration sector may be lost after interrupted erase/program.
 * Production requires sector endurance and brownout qualification.
 */
#include "util_backend.h"
#include "cfg_flash_plan.h"
#include "cfg_flash_writer.h"
#include "motor_backend.h"
#include "motor_safety.h"
#include "motor_nfault_trip.h"
#include "gpio_runtime.h"
#include "pb14_bidir_tx.h"
#include <dl_flash.h>
#include <stdint.h>
#include <stddef.h>

#define FW1_CFG_START UINT32_C(0x4000)
#define FW1_CFG_END   UINT32_C(0x5000)
#define FW1_CFG_MAX   (FW1_CFG_END-FW1_CFG_START)
#define RAM_C_START   UINT32_C(0x00c18000)
#define RAM_C_END     UINT32_C(0x00c20000)
_Static_assert(DL_FLASH_SECTOR_SIZE==UINT32_C(2048) &&
               FW1_CFG_MAX==UINT32_C(4096),
               "FW1 flash/sector contract differs from TI SDK");

extern uint8_t __ramfunct_start__[];
extern uint8_t __ramfunct_end__[];

/* The application startup/linker MUST copy SDK .TI.ramfunc to SRAM_C.
 * This is checked at runtime, and must also be verified in ELF/MAP.
 */
static uint8_t snapshot[FW1_CFG_MAX] __attribute__((aligned(8)));
static volatile uint32_t committing;

static int flash_ready(void)
{
    const uintptr_t ram_start=(uintptr_t)__ramfunct_start__;
    const uintptr_t ram_end=(uintptr_t)__ramfunct_end__;
    return ram_start>=RAM_C_START && ram_end>ram_start &&
           ram_end<=RAM_C_END && !am13e_app_nfault_asserted() &&
           am13e_app_motor_nfault_trip_ready() &&
           am13e_app_motor_inactive_preflight_ok() &&
           !am13e_pb14_bidir_tx_busy();
}

/* Only the device driverlib command-polling function executes during
 * actual flash busy. Never run other Flash-resident instructions or any
 * ISR in that interval. Other ISR traffic may run BETWEEN commands.
 */
static int erase_2k(uint32_t addr,void *ctx)
{
    if(ctx!=(void*)&committing || addr<FW1_CFG_START ||
       addr>=FW1_CFG_END || (addr&(DL_FLASH_SECTOR_SIZE-1U))!=0U)
       return 0;
    const uint32_t ps=__get_PRIMASK();
    __disable_irq();
    int ok=committing && flash_ready() &&
           DL_Flash_eraseSector(addr)==DL_FLASH_SUCCESS;
    __set_PRIMASK(ps);
    return ok;
}
static int program_16(uint32_t addr,const uint32_t data[4],void *ctx)
{
    if(ctx!=(void*)&committing || data==NULL ||
       addr<FW1_CFG_START || addr>FW1_CFG_END-16U || (addr&15U))
       return 0;
    const uint32_t ps=__get_PRIMASK();
    __disable_irq();
    int ok=committing && flash_ready() &&
           DL_Flash_program(addr,(uint8_t*)(uintptr_t)data,16U)==
             DL_FLASH_SUCCESS;
    __set_PRIMASK(ps);
    return ok;
}
static int readback_16(uint32_t addr,uint8_t out[16],void *ctx)
{
    if(ctx!=(void*)&committing || out==NULL ||
       addr<FW1_CFG_START || addr>FW1_CFG_END-16U || (addr&15U))
       return 0;
    /* Called after TI DriverLib reports programming + ECC verify PASS.
     * The live Flash readback also validates the exact padded bytes.
     */
    const volatile uint8_t *flash=(const volatile uint8_t*)(uintptr_t)addr;
    for(unsigned i=0U;i<16U;++i) out[i]=flash[i];
    return 1;
}

int am13e_app_cfg_commit(const void *destination,const void *source,
                         unsigned int byte_count)
{
    AM13E_CfgFlashPlan source_plan,staged_plan;
    const uintptr_t dst=(uintptr_t)destination,src=(uintptr_t)source;
    if (!am13e_cfg_flash_plan(dst,src,byte_count,&source_plan) ||
        byte_count>FW1_CFG_MAX) return 0;
    const uint32_t ps=__get_PRIMASK();
    __disable_irq();
    /* Rel17 only calls savecfg() with ertm==0 and busy==0. Drag/Lock
     * Brake may nevertheless have left MCPWM0's internal counter active.
     * Move to the already-implemented safe Stop state BEFORE Flash
     * erase; this never activates PB13 or a physical PWM pad.
     */
    if (committing || am13e_app_nfault_asserted() ||
        !am13e_app_motor_nfault_trip_ready() ||
        am13e_pb14_bidir_tx_busy()) {
        __set_PRIMASK(ps);
        return 0;
    }
    am13e_app_motor_commutation_enable(0);
    if (!flash_ready()) {
        __set_PRIMASK(ps);
        return 0;
    }
    /* Reserve the transaction and take a coherent snapshot. A command
     * arriving later via PB14 cannot silently change mid-write payload.
     */
    committing=1U;
    for(unsigned i=0U;i<byte_count;++i)
        snapshot[i]=((const uint8_t*)source)[i];
    if (!am13e_cfg_flash_plan(dst,(uintptr_t)snapshot,byte_count,
                              &staged_plan) ||
        staged_plan.byte_count!=source_plan.byte_count ||
        staged_plan.padded_program_bytes!=source_plan.padded_program_bytes) {
        committing=0U;
        __set_PRIMASK(ps);
        return 0;
    }
    __set_PRIMASK(ps);

    /* Same settings already persisted: no needless erase/program wear. */
    int same=1;
    const volatile uint8_t *stored=(const volatile uint8_t*)destination;
    for(unsigned i=0U;i<byte_count;++i)
        if(stored[i]!=snapshot[i]) {same=0;break;}
    if (!same) {
        const AM13E_CfgFlashOps ops={
            .erase_sector=erase_2k,.program_ecc16=program_16,
            .read_ecc16=readback_16,.ctx=(void*)&committing
        };
        same=am13e_cfg_flash_execute(&staged_plan,snapshot,&ops);
        /* Whole payload verification also detects an unexpected
         * modification between ECC-sized blocks.
         */
        if(same)
            for(unsigned i=0U;i<byte_count;++i)
                if(stored[i]!=snapshot[i]) {same=0;break;}
    }
    const uint32_t finish_mask=__get_PRIMASK();
    __disable_irq();
    /* Never mark a torn/partially verified transaction successful. */
    committing=0U;
    __set_PRIMASK(finish_mask);
    return same;
}
