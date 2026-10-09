# AM13E Application Clock Requirement — External 8 MHz Crystal

Status: **input requirement recorded; actual XTAL clock backend NOT
implemented; no flashable Rel17 Application image.**

## User-selected clock reference

- **External XTAL: 8,000,000 Hz**.
- Crystal / resonator connected between **X1 and X2**, not a
  TTL/CMOS clock injected into single-ended `HFCLK_IN`.
- This defines the oscillator **reference**, not the final
  Cortex-M33 CPU `MCLK`.
- CPU `MCLK` rate, PLL multiplication/division, flash wait
  configuration, HSCLK/peripheral clock divisors, and motor PWM
  timer clock are **not specified**. Do NOT invent any of them.
- Keep existing Boot startup on internal 32 MHz SYSOSC; Boot is
  independent. The Application must switch away only after
  XTAL startup is actually qualified.
- SysTick target remains **16 kHz**. Reload cycles are derived
  from the backend-reported *actual configured MCLK* Hz.

## Documentation discrepancy — must resolve before oscillator programming

Both supplied TI documents show Revision dates in August 2026:

1. AM13E23019 **Datasheet Rev A, SPRSPC3A**, feature overview
   says *external 4–25 MHz crystal oscillator*; section 6.8.2.2
   gives XTAL electrical operating range *4–48 MHz*. Both
   include **8 MHz**.
2. AM13E230x **TRM Rev B, SPRUJF2B**, §3.4.2.4 and its
   clock block diagrams explicitly state *10–25 MHz XTAL*,
   which **does not include 8 MHz**.

Do not conflate this with the separate digital clock input
`HFCLK_IN` (4–48 MHz), which is **not** an 8 MHz quartz crystal.

The electrical operating-range contradiction requires TI confirmation
for the exact **AM13E23019 device/silicon revision** and actual
crystal drive/load/startup characteristics. Datasheet electrical
specs are more specific, but the conflicting TRM is not ignored.
Until resolved, do not claim the crystal circuit is hardware qualified.

## SDK primitives inspected

- `DL_SYSCTL_setHFCLKSourceXTAL(startupTime, monitorEnable)`:
  powers up the XTAL and optionally waits for `HFCLKGOOD`.
  The SDK documents required XTAL **pad/IOMUX** configuration
  independently, and `startupTime` has 64 µs resolution.
- `DL_SYSCTL_configSYSPLL()`: accepts
  `DL_SYSCTL_SYSPLL_REF_HFCLK` and reference-frequency range.
- `DL_SYSCTL_switchMCLKfromSYSOSCtoHSCLK(...)`: switches MCLK
  only after the HSCLK source has been enabled and stabilized.
- Actual system frequency and motor timing must be confirmed
  with appropriate device measurements (e.g. FCC), not by
  a hard-coded assumed MHz value.

## Current source boundary

`mcu/AM13E/clock_backend.h` declares, but does NOT define:

```c
uint32_t am13e_app_clock_configure_xtal8(void);
```

The board implementation must configure X1/X2, start/monitor XTAL,
select an explicitly approved direct or PLL MCLK and return its
verified Hz; return zero or halt safely if any condition fails.
No weak/no-op implementation is allowed.

`mcu/AM13E/system_runtime.c` checks Boot's inherited internal
SYSOSC only as a **handoff precondition**; it then calls the
board clock backend and requires HSCLK to be selected, with
`HFCLKGOOD` present. The 16 kHz SysTick period is derived
from the backend-reported MCLK, never from 32 MHz nor blindly
from the 8 MHz XTAL frequency.

**Until the real backend exists, the open symbol
`am13e_app_clock_configure_xtal8` intentionally blocks ELF linking.**
The Application is not yet motor-ready.

### Open question

**What is the required Cortex-M33 MCLK target frequency?**
Examples of architecturally distinct policies include:
- XTAL directly to MCLK = 8 MHz (not motor-performance qualified)
- 8 MHz XTAL as SYSPLL reference, with a separately approved
  high-frequency MCLK for Rel17 motor control

No PLL parameters will be selected from guesses.
