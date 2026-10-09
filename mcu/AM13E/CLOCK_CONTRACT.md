# AM13E Application Clock Requirement — LaunchPad 25 MHz XTAL

Status: **25 MHz XTAL / 200 MHz nominal SYSPLL clock backend: 11/11 ARM Objects PASS** (GCC 15.2.1). Both actual TI DriverLib dependency objects resolve. The real TI Flash wait-state RAMFUNC also passed a **non-flashable synthetic Linker/Vector fixture**. Full Rel17 Application ELF and on-silicon clock/power-stage qualification remain pending.

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

## Selected Clock Plan — 25 MHz XTAL / SYSPLL / MCLK 200 MHz

The user selected **200 MHz MCLK**, with a 25 MHz Y1 reference.
For the AM13E23019 200MHz-grade device and TI SDK 26.01.00:

| Quantity | Selection | Grounding |
| --- | --- | --- |
| External quartz crystal | 25 MHz, LaunchPad Y1 | SLVUDH9 §2.4 |
| SYSPLL reference | HFCLK from XTAL | AM13E TRM §3.4.2.3 |
| PDIV | ÷2, register 0x1 | 25/2 = 12.5MHz feedback input |
| inputFreq lookup | 8..16MHz (factory LUT) | DL_SYSCTL_SYSPLL_INPUT_FREQ_8_16_MHZ |
| QDIV | effective ×32, register **31 (0x1F)** | TRM effective multiplier = QDIV + 1 |
| VCO | 400 MHz | 12.5×32 |
| RDIVCLK0 | ÷2, register 0 | SYSPLLCLK0=200MHz |
| RDIVCLK1 | disabled | No peripheral requirement established |
| MCLK | **200 MHz**, nominal configuration | HW measurement pending |
| MCLK2 | 100MHz (÷2) | PD1 max |
| MCLK4 / ULPCLK | 50MHz (÷4) | PD0 max |
| Flash RWAIT | **3** before switching to 200MHz | SPRSPC3A Table 6-1 |
| SysTick | 16kHz, 12,500 clock cycles | Rel17 16kHz tick |

The configured 200MHz value is checked using source/lock/readback
registers; **the physical frequency has not been measured**.
TI DriverLib `DL_FRI_setReadWaitStates(3)` is a RAMFUNC, so its
object and startup SRAM-copy contract must be included in eventual
real Application linking, before `init()` executes. No dummy
Flash wait-state function is permitted.

## Source implementation and remaining qualification

`mcu/AM13E/clock_xtal25_pll200.c` implements the accepted clock
sequence for the LaunchPad-reference Y1 on X1/X2. Unlike TI's
high-level helper functions with unbounded internal busy-waits,
this implementation uses SDK register definitions and finite
poll budgets at XTAL, SYSPLL, and MCLK transition stages. Poll
budgets are NOT calibrated elapsed-time guarantees.

- XTAL startup monitor nominally set to 156×64µs = 9.984ms.
  The TRM gives 5–10ms as typical, but this **must be qualified**
  with the final crystal, capacitors, temperature and board.
- Configure XTAL double-ended mode, reject external digital
  HFCLK_IN selection, wait for HFCLKGOOD.
- Program factory SYSPLL LUT for fLOOPIN=12.5MHz and confirm
  SYSPLLGOOD before switching CPU MCLK to SYSPLLCLK0.
- Raise Flash RWAIT to 3, set MCLK2÷2 and MCLK4÷4 **before**
  the 200MHz switch.
- Preserve Boot PRIMASK=1. If a prerequisite fails, return 0 and
  stop in the existing fail-closed Application init path.
- No MCPWM, ADC, comparator, gate-driver, LED pinmux or IRQ
  safety activation takes place in this clock module.

**P0 hardware gaps:** oscillator layout/actual X1-X2 pin allocation,
oscillator startup and lock measurement, PLL clock precision/FCC,
temperature/voltage corners and real Clock Domain timing. The
status checks validate the configured source, not independent
silicon clock accuracy.

Current CMake target is still object-only, not a runnable ELF.
ARM GCC 15.2.1 Object Compile of the PLL backend was confirmed by the user-provided `e1c-pll200-build.log`. This validates C compilation, not the configuration on silicon. The board-safe
`am13e_app_motor_runtime_enable_interrupts()` remains
an unresolved and mandatory platform Link barrier.

## Real TI DriverLib dependencies — compile + synthetic RAMFUNC linker probe PASS

The latest **11/11** GCC 15.2.1 Application Object build passed.
The cross-object `nm` audit resolved **46** references and reported
**60** still undefined; both previously unresolved TI functions are
now satisfied by genuine TI SDK objects:

- `DL_Common_delayCycles` — defined in
  `source/driverlib/am13e230x/dl_common.c`.
- `DL_FRI_setReadWaitStates` — defined in
  `source/driverlib/am13e230x/dl_fri.c`, tagged `.TI.ramfunc`.

Both **genuine SDK source files** are in the AM13E Application
Object Target, and their real objects compiled under the selected
toolchain. The WSL `e1c-real-fri-ramfunc-link-probe.log` confirms
that **real** `DL_FRI_setReadWaitStates` resides inside SRAM_C
`.TI.ramfunc` with a distinct Flash LMA in the non-flashable
synthetic linker fixture, alongside the real TI Startup/Vector mapping.
The actual full Rel17 Application link must independently verify
the production MAP/ELF placement and startup handoff.

The clock code also now checks retained SYSPLL/XTAL GOOD-or-OFF
status before disabling them (per the DriverLib reset-state
guidance). The bounded poll counts are not hard real-time deadlines
and XTAL X1/X2 board pad/IOMUX initialization remains unimplemented.

## Firmware vs physical hardware qualification

The TI LaunchPad reference is sufficient to define a baseline for
clock-source *code*, not proof that an unbuilt custom board has Y1
routed, correctly loaded or starts within 10ms. Do not assume
future customer hardware repeats the LaunchPad BOM and layout.
Do not issue a motor-enable recommendation based on Clock Source
compile alone. WSL compilation, map/link checks, FCC measurement,
and on-board oscilloscope/clock validation remain separate gates.
