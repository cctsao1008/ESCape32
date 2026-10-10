# AM13E ESCape32 Application — Clock Integration Contract

**Authority:** Product HW Architecture Baseline v1.6 for physical
clock-source pins; ESCape32 Rel17 for application behavior; device
TRM/datasheet and TI DriverLib for *how* to program MCU registers.

**Last verified:** 11/11 ARM GCC 15.2.1 objects PASS; real TI
`DL_FRI_setReadWaitStates()` passed a synthetic RAMFUNC linker test.
No complete Rel17 Application ELF or on-silicon clock test exists.

## Architecture-owned hardware clock input

HW Baseline v1.6 reserves **PC16_X1 / PC17_X2** for an external
**HFXT crystal**, and excludes single-ended HFCLKIN. Pin ownership
and package compatibility are from that **product HW baseline**,
not from a TI evaluation board.

HW Baseline v1.6 does **not** itself freeze the XTAL frequency
or the CPU MCLK. The following frequencies are **subsequent
accepted detailed implementation choices**:

| Item | Current firmware implementation choice |
| --- | --- |
| External HFXT frequency | **25 MHz** |
| SYSPLL source | HFXT / HFCLK |
| PDIV | ÷2; effective PLL input 12.5 MHz |
| QDIV | encoded 31 = effective ×32 |
| VCO | 400 MHz |
| SYSPLL CLK0 RDIV | ÷2 |
| Cortex-M33 MCLK | **200 MHz** |
| MCLK2 | 100 MHz |
| MCLK4 / ULPCLK | 50 MHz |
| Flash read wait states | 3 before MCLK switch |
| Rel17 SysTick | 16 kHz, 12,500 MCLK cycles |

These choices are an implementation policy, **not an amendment
to the HW Architecture Baseline v1.6** and not evidence of
oscillator or PLL operation on a physical board.

## ESCape32-first source ownership

- `src/main.c` and the other original ESCape32 Rel17 sources
  own FW1 control and application behavior.
- `mcu/AM13E/system_runtime.c`,
  `mcu/AM13E/clock_source_reference.c` and
  `mcu/AM13E/clock_backend.h` own **AM13E MCU adaptation**.
- TI SDK/CMSIS/DriverLib supply low-level peripheral access
  just as libopencm3 supports existing STM32 targets.
  SDK motor-control examples, their SysConfig output, and their
  standalone build are **not** an Application architecture baseline.
- The existing Boot port is relevant only for the shared
  Boot-to-Application interface, Flash programming and
  startup/VTOR reference; Boot implementation details do
  **not** silently override the SW Flash/update architecture.

The canonical build remains **`add_target(AM13E AM13E)`**
from the ESCape32 root CMake project.

## Current clock code and safety boundary

`clock_source_reference.c` uses real TI registers and DriverLib,
checks inherited SYSOSC, increases Flash RWAIT before changing
MCLK, uses finite software polls for oscillator/PLL status,
and returns the configured **nominal** 200 MHz only after
clock-source/lock/readback checks. `system_runtime.c`
configures 16 kHz SysTick from this agreed MCLK.

TI `DL_FRI_setReadWaitStates` is genuine `.TI.ramfunc`:
the latest synthetic linker fixture verified its SRAM_C
execution VMA and distinct application-Flash LMA.
This is **not** a production Application MAP/ELF gate.

Outstanding for on-target bring-up: HFXT X1/X2 IOMUX/pad
setup, crystal electrical/load/startup qualification,
bounded-time failure handling, observed PLL lock and
actual frequency, power-domain/Flash corners, PRIMASK
safe unmask, and power-stage fault safety.
No MCU board has been validated.

## Required build check after source/CMake realignment

After pulling the latest branch, rebuild from the ESCape32
root; inspect the compile database to ensure no device
example-board SysConfig directory is inherited. Toolchain
and object/link tests must remain subordinate to actual
ESCape32 Application integration.
