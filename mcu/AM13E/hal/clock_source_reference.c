/*
 * ESCape32 Rel17 AM13E23019 — 25MHz XTAL -> 200MHz CPU clock.
 *
 * Target: ESCape32 Rel17 AM13E23019; TI SDK is the MCU driver layer.
 * This configures genuine SYSCTL/FRI registers; no mock CLK source.
 *
 * SYSPLL formula: 25MHz / 2 * (31 + 1) / 2 = 200MHz.
 *    PDIV raw = 1  (/2)
 *    QDIV raw = 31 (effective x32)
 *    RDIVCLK0 raw = 0 (/2)
 *    VCO = 400MHz
 *
 * The TI SDK's high-level XTAL/PLL/MCLK transition helpers have
 * unbounded busy-wait loops. This backend uses their *documented*
 * register configuration sequence with finite polling instead.
 * Poll limits are loop budgets, NOT precisely calibrated timeouts.
 * Board-specific 25MHz startup/load and real clock measurement
 * remain necessary. Never start power-stage outputs based on this
 * code alone. Boot hands off with PRIMASK=1; this does not unmask.
 */
#include "clock_backend.h"
#include <soc.h>
#include <dl_sysctl.h>
#include <dl_fri.h>
#include <dl_common.h>

#define AM13E_PLL_QDIV_RAW              (31U)
#define AM13E_XTAL_STARTUP_64US_TICKS   (156U) /* 9.984ms nominal; board qualification pending */
#define AM13E_XTAL_POLL_LIMIT           (4000000U)
#define AM13E_PLL_POLL_LIMIT            (1000000U)
#define AM13E_MCLK_POLL_LIMIT           (1000000U)

_Static_assert(AM13E_APP_XTAL_HZ / 2U * (AM13E_PLL_QDIV_RAW + 1U) ==
                   AM13E_APP_VCO_HZ,
               "XTAL/PDIV/QDIV do not generate 400MHz VCO");
_Static_assert(AM13E_APP_VCO_HZ / 2U == AM13E_APP_MCLK_HZ,
               "SYSPLLCLK0 must generate exactly 200MHz");
_Static_assert(AM13E_APP_MCLK_HZ / AM13E_APP_SYSTICK_HZ == 12500U,
               "Rel17 16kHz tick period must be exact");
_Static_assert(AM13E_APP_MCLK_HZ % AM13E_APP_SYSTICK_HZ == 0U,
               "Rel17 SysTick rate must divide MCLK");

static int wait_clock_good(uint32_t mask, uint32_t required, uint32_t budget)
{
    for (uint32_t i = 0U; i < budget; ++i) {
        if ((DL_SYSCTL_getClockStatus() & mask) == required) {
            return 1;
        }
    }
    return 0;
}

/* The TI SYSCTL contract requires an oscillator/PLL to have reached
 * its stable GOOD or OFF state before being disabled. Lower reset
 * levels can retain configuration from a prior application instance.
 * This is only a finite software-poll budget, not a timed timeout.
 */
static int wait_clock_good_or_off(uint32_t good, uint32_t off, uint32_t budget)
{
    for (uint32_t i = 0U; i < budget; ++i) {
        if ((DL_SYSCTL_getClockStatus() & (good | off)) != 0U) {
            return 1;
        }
    }
    return 0;
}

uint32_t am13e_app_clock_configure_xtal25(void)
{
    /* Do not switch a running motor controller's clocks or unmask IRQ. */
    if (__get_PRIMASK() == 0U ||
        DL_SYSCTL_getMCLKSource() != DL_SYSCTL_MCLK_SOURCE_SYSOSC ||
        (DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSOSCFREQ_MASK) !=
            SYSCTL_CLKSTATUS_SYSOSCFREQ_SYSOSC32M) {
        return 0U;
    }

    /* Important: CPUCLK up to 200MHz requires RWAIT >= 3 according
     * to SPRSPC3A Table 6-1. The TI FRI API is RAMFUNC; it must be
     * linked with the actual TI .TI.ramfunc copy-at-reset contract.
     * Increase wait states BEFORE switching to the fast clock.
     */
    DL_FRI_setReadWaitStates(3U);
    if (DL_FRI_getReadWaitStates() < 3U) {
        return 0U;
    }
    /* Keep PD0 ULPCLK <=50MHz and PD1 peripheral clock <=100MHz. */
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIV_2_DIV_4);

    /* Ensure the XTAL is off and a prior retained digital HFCLK_IN
     * selection cannot masquerade as the Y1 crystal.
     */
    if (!wait_clock_good_or_off(SYSCTL_CLKSTATUS_HFCLKGOOD_MASK,
                                SYSCTL_CLKSTATUS_HFCLKOFF_MASK,
                                AM13E_XTAL_POLL_LIMIT)) {
        return 0U;
    }
    SYSCTL->SOCLOCK.XTALCR |= SYSCTL_XTALCR_OSCOFF_MASK;
    if (!wait_clock_good(SYSCTL_CLKSTATUS_HFCLKOFF_MASK,
                         DL_SYSCTL_CLK_STATUS_HFCLK_OFF,
                         AM13E_XTAL_POLL_LIMIT)) {
        return 0U;
    }

    /* Crystal / two-pin mode, NOT HFCLK_IN single-ended mode.
     * This does not configure user board routing/load capacitors.
     * X1/X2 are reserved for HFXT by the HW architecture; no GPIO reuse.
     */
    SYSCTL->SOCLOCK.XTALCR &= ~SYSCTL_XTALCR_SE_MASK;
    SYSCTL->SOCLOCK.HSCLKEN &= ~SYSCTL_HSCLKEN_USEEXTHFCLK_MASK;
    DL_SYSCTL_setXTALStartupTime(AM13E_XTAL_STARTUP_64US_TICKS);
    DL_SYSCTL_enableHFCLKStartupMonitor();
    SYSCTL->SOCLOCK.XTALCR &= ~SYSCTL_XTALCR_OSCOFF_MASK;
    if (!wait_clock_good(SYSCTL_CLKSTATUS_HFCLKGOOD_MASK,
                         DL_SYSCTL_CLK_STATUS_HFCLK_GOOD,
                         AM13E_XTAL_POLL_LIMIT)) {
        return 0U;
    }

    /* SYSPLL reference remains external HFCLK (25MHz).
     * SYSOSC must still be running at its base 32MHz rate.
     * SDK DL_SYSCTL_configSYSPLL() cannot be used as-is here because
     * its startup loop is unbounded; mirror its validated register
     * sequence with explicit limits.
     */
    if (!wait_clock_good_or_off(SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK,
                                SYSCTL_CLKSTATUS_SYSPLLOFF_MASK,
                                AM13E_PLL_POLL_LIMIT)) {
        return 0U;
    }
    DL_SYSCTL_disableSYSPLL();
    if (!wait_clock_good(SYSCTL_CLKSTATUS_SYSPLLOFF_MASK,
                         DL_SYSCTL_CLK_STATUS_SYSPLL_OFF,
                         AM13E_PLL_POLL_LIMIT)) {
        return 0U;
    }

    DL_Common_updateReg(
        &SYSCTL->SOCLOCK.SYSPLLCFG0,
        ((uint32_t)DL_SYSCTL_SYSPLL_RDIVCLK0_DIV2 <<
             SYSCTL_SYSPLLCFG0_RDIVCLK0_OFS) |
            ((uint32_t)DL_SYSCTL_SYSPLL_RDIVCLK1_DIV2 <<
             SYSCTL_SYSPLLCFG0_RDIVCLK1_OFS) |
            DL_SYSCTL_SYSPLL_CLK0_ENABLE |
            DL_SYSCTL_SYSPLL_CLK1_DISABLE |
            (uint32_t)DL_SYSCTL_SYSPLL_REF_HFCLK,
        SYSCTL_SYSPLLCFG0_RDIVCLK0_MASK |
            SYSCTL_SYSPLLCFG0_RDIVCLK1_MASK |
            SYSCTL_SYSPLLCFG0_ENABLECLK0_MASK |
            SYSCTL_SYSPLLCFG0_ENABLECLK1_MASK |
            SYSCTL_SYSPLLCFG0_SYSPLLREF_MASK);
    DL_Common_updateReg(
        &SYSCTL->SOCLOCK.SYSPLLCFG1,
        ((uint32_t)AM13E_PLL_QDIV_RAW << SYSCTL_SYSPLLCFG1_QDIV_OFS),
        SYSCTL_SYSPLLCFG1_QDIV_MASK);
    DL_Common_updateReg(
        &SYSCTL->SOCLOCK.SYSPLLCFG1,
        (uint32_t)DL_SYSCTL_SYSPLL_PDIV_2,
        SYSCTL_SYSPLLCFG1_PDIV_MASK);

    /* SYSPLLPARAM is factory-tuned for feedback input 12.5MHz,
     * which belongs to the TI 8..16MHz lookup bin.
     * Exactly follows DL_SYSCTL_configSYSPLL's FACTORY copy.
     */
    const uintptr_t table =
        (uintptr_t)DL_SYSCTL_SYSPLL_INPUT_FREQ_8_16_MHZ;
    SYSCTL->SOCLOCK.SYSPLLPARAM0 = *(volatile const uint32_t *)table;
    SYSCTL->SOCLOCK.SYSPLLPARAM1 =
        *(volatile const uint32_t *)(table + sizeof(uint32_t));

    SYSCTL->SOCLOCK.HSCLKEN |= SYSCTL_HSCLKEN_SYSPLLEN_ENABLE;
    if (!wait_clock_good(SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK,
                         DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD,
                         AM13E_PLL_POLL_LIMIT)) {
        return 0U;
    }

    /* Do not update HSCLK mux after it has become the running MCLK. */
    DL_SYSCTL_setHSCLKSource(DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
    if (!wait_clock_good(SYSCTL_CLKSTATUS_HSCLKGOOD_MASK,
                         DL_SYSCTL_CLK_STATUS_HSCLK_GOOD,
                         AM13E_MCLK_POLL_LIMIT)) {
        return 0U;
    }
    SYSCTL->SOCLOCK.MCLKCFG |= SYSCTL_MCLKCFG_USEHSCLK_ENABLE;
    DL_Common_delayCycles(20U);
    if (!wait_clock_good(SYSCTL_CLKSTATUS_HSCLKMUX_MASK,
                         DL_SYSCTL_CLK_STATUS_MCLK_SOURCE_HSCLK,
                         AM13E_MCLK_POLL_LIMIT)) {
        return 0U;
    }

    /* Register readback/status-qualified NOMINAL MCLK, not a
     * frequency-counter or oscilloscope measurement of silicon.
     */
    if ((SYSCTL->SOCLOCK.SYSPLLCFG1 & SYSCTL_SYSPLLCFG1_QDIV_MASK) !=
            (AM13E_PLL_QDIV_RAW << SYSCTL_SYSPLLCFG1_QDIV_OFS) ||
        (SYSCTL->SOCLOCK.SYSPLLCFG1 & SYSCTL_SYSPLLCFG1_PDIV_MASK) !=
            (uint32_t)DL_SYSCTL_SYSPLL_PDIV_2 ||
        (SYSCTL->SOCLOCK.SYSPLLCFG0 &
             SYSCTL_SYSPLLCFG0_RDIVCLK0_MASK) !=
            ((uint32_t)DL_SYSCTL_SYSPLL_RDIVCLK0_DIV2 <<
                SYSCTL_SYSPLLCFG0_RDIVCLK0_OFS) ||
        DL_FRI_getReadWaitStates() < 3U) {
        return 0U;
    }
    return AM13E_APP_MCLK_HZ;
}
