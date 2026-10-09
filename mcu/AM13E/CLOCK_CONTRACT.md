# AM13E Application Clock Requirement — LaunchPad 25 MHz XTAL

Status: **25 MHz external crystal reference selected**. Application
XTAL/PLL clock backend is declaration-only; no board startup, motor
operation or complete Application ELF has been validated.

## Primary evidence — TI LP-AM13E230

TI **AM13E230x LaunchPad™ Development Kit User's Guide**, SLVUDH9,
March 2026, **section 2.4 "Clock", page 13**, Figure 2-6:

- The LaunchPad default external reference is a **25 MHz crystal
  oscillator Y1**, across MCU **X1 (PC16)** and **X2 (PC17)**.
- Separate external clock option: **HFCLK_IN**, a **4–48 MHz digital
  clock** applied via header **J14**.
- The documented HFCLK_IN option requires removing **R1 and R18**
  (isolate Y1), populating **R19** and driving **J14**.
- **Our design uses the 25 MHz Y1 XTAL architecture**, not a
  single-ended 25 MHz digital clock injected through J14.
- This is a LaunchPad *reference architecture*, NOT confirmation of
  actual customer-board pin routing, crystal BOM, capacitors or layout.

The 25 MHz reference is within both previously reviewed crystal
ranges: AM13E23019 Datasheet Rev A feature specification **4–25 MHz**
and AM13E230x TRM Rev B XTAL discussion **10–25 MHz**. The 8 MHz
frequency-range discrepancy described in the previous revision of
this file is no longer relevant to the selected 25 MHz design.
This agreement does NOT waive electrical/load/startup validation
for the specific MCU/package, crystal and PCB.

## Selected / undetermined parameters

| Parameter | State |
| --- | --- |
| External oscillator element | **25,000,000 Hz XTAL crystal (Y1 reference)** |
| XTAL connection | **X1 (PC16) / X2 (PC17)** |
| External digital HFCLK_IN | **Not selected** |
| Boot handoff clock | Existing **SYSOSC 32 MHz** precondition; Boot unchanged |
| Application MCLK | **OPEN: no nominal CPU rate specified** |
| SYSPLL | **OPEN: multiplier/divider and power/Flash rules unselected** |
| Motor PWM / MCPWM clock | **OPEN: hardware/board-specific** |
| Rel17 SysTick | **16 kHz**, derived from verified actual MCLK |
| Safe IRQ unmask | Separate board safety barrier, NOT XTAL backend |

A **25 MHz crystal frequency is not the Cortex-M33 MCLK by itself**.
Before configuring SYSPLL, determine the requested MCLK and valid
reference-to-PLL output ratios using TI device/SDK constraints.
Do not infer a 25 MHz CPU clock or invent a 160/180/200 MHz target.

## SDK responsibilities, not yet implemented

1. Board-confirm the physical crystal, required load capacitors and
   X1/X2 pad configuration. Never copy the LaunchPad's component
   routing into an unverified custom PCB.
2. Configure XTAL IOMUX and its drive/startup according to TI SDK;
   `DL_SYSCTL_setHFCLKSourceXTAL(startupTime, monitorEnable)`
   is the XTAL startup interface. Select a bounded startup/fault
   policy based on real component data.
3. Verify the HFCLKGOOD status **before** switching MCLK.
4. If SYSPLL is selected, set an explicitly approved configuration
   using the 25 MHz reference within TI operating limits.
5. Switch to the intended HSCLK/MCLK and return the **verified CPU
   MCLK frequency** in Hz; fail closed on any mismatch.
6. Preserve disabled motor outputs and Boot PRIMASK throughout
   the clock transition. The board-only safe IRQ barrier enables
   interrupts after full system qualification.

The current `mcu/AM13E/clock_backend.h` deliberately declares only:

```c
#define AM13E_APP_XTAL_HZ UINT32_C(25000000)
uint32_t am13e_app_clock_configure_xtal25(void);
```

`mcu/AM13E/system_runtime.c` calls this **undefined** backend
after verifying Boot's inherited SYSOSC precondition. It checks
the required HSCLK source and HFCLKGOOD condition and calculates
the 16 kHz SysTick reload from the returned verified MCLK. It does
not program XTAL pinmux, SYSPLL, or motor clocks.

A *real* backend must implement the declaration. Do NOT replace it
with a dummy 25 MHz constant or an unverified clock switch merely to
achieve ELF Linking. At this integration phase an unresolved external
clock backend is expected and intentional.

## Remaining clock decision

**Cortex-M33 target MCLK** needs to be chosen separately from XTAL.
Once chosen, validate SYSPLL limits, voltage/Flash timing, clocks
used by MCPWM/ADC/BEMF, and interrupt timing against TI collateral.
