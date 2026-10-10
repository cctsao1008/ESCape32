# ESCape32 on AM13E23019 — generic platform direction

## Architecture boundary

The reusable port is **AM13E MCU backend + ESCape32 application logic**.
A reference ESC design may be used to exercise the backend; it is **not**
the default or authoritative definition of an AM13E23019 board.

| Layer | Responsibility | Not allowed as implicit default |
| --- | --- | --- |
| Portable control logic | Rel17 commutation/AQ/duty/clock arithmetic, motor-audio plans, DShot codec, ADC math | Hard-coded pinmux, divider, NTC, gate polarity |
| AM13E MCU backend | Real TI DriverLib, M33 startup, MCPWM/ECAP/ADC/GPIO/Flash primitives | Assuming a particular ESC hardware schematic |
| Board profile | MCU pad routes, comparator paths, gate driver, active levels, RED/FED, external fault, crystal, analog scaling | Guessed electrical values or auto-arming |
| Image/update profile | Flash regions, boot entry/VTOR, Cfg.id/Vector Boot validity and per-frame wire CRC, delivery transport | Assuming every AM13E board shares a boot partition |
| Application | ESCape32 Rel17 policy, input/arming, protection, telemetry and music | Claiming hardware-verified behavior from host tests |

## Current build targets

- `AM13E_PORTABLE_LOGIC`: a **board-neutral ARM object compile gate**.
  It compiles code that takes explicit values or produces control plans;
  no board I/O is configured. It is not an ELF to flash.
- `AM13E_FW1_REL17_IMAGE` and `BOOT5_PB14.elf`: **existing reference
  integration**. These are real linked images but they encode a fixed
  0x6000 APP location and fixed reference pin assignments. They must
  not be described as a generic AM13E board support package.
- Reference FW1 runs the G431-derived MCPWM / RED/FED / NTC / VBUS
  **software** model, but the five user-deferred features are IO-only:
  PB13 Gate Enable never activates, PB15 nFAULT has no interrupt/Trip,
  independent OC/OST2 is absent, UART TX/current-limit have no routes.
  Six physical motor output pads stay disconnected.

## First generic build gate

```bash
cd ~/github/ESCape32
cmake -B build-am13e-rel17 -D AM13E_SDK_ROOT="$HOME/ti/am13e230x_sdk_26_01_00_03"
cmake --build build-am13e-rel17 --target AM13E_PORTABLE_LOGIC --parallel 4
```

### Next source refactoring work

1. Introduce an independently selectable board descriptor for input
   routes, sensing, and motor outputs; avoid treating PB14, PB13, and
   PA6/PA28 as universal device defaults.
2. Decouple CPU/peripheral init from the reference crystal and timer
   topology, keeping concrete DriverLib register checks.
3. Isolate the Boot/APP linker layout and image metadata behind an
   explicit image profile; **the approved original Rel17 Cfg.id/vector launch ABI replaces the retired v1.6 image CRC; host updater and recovery changes still require validation**.
4. Move reference pin assignment behind an opt-in BSP and establish
   a board-free automated build/test gate.
5. Keep Motor Drive and Motor Audio on the same MCPWM resource with
   exclusive ownership; neither is a standalone GPIO buzzer.

This document is a porting boundary, not a hardware qualification.

## MCU-side fault trip source separation

- `fault_trip_backend.[ch]` is a **board-neutral TI DriverLib backend** with
  explicit MCPWM/XBAR route parameters, route consistency checks and actual
  register/status readback. It never selects pins, arms outputs or clears OST.
- `motor_fault_route.c` remains an **unlinked reference design**
  for a future PB15 -> OST1 function; it is deliberately not compiled
  into the IO-only FW1. PB15 is initialized as plain GPIO Input only.
- `AM13E_MCU_FAULT_TRIP_BACKEND` is an independent board-free ARM object
  compile gate. CI also compiles the full reference firmware, uses ESCape32
  CMake + Unix Makefiles, and now treats host regression failures as fatal.
- This is not a universal runnable firmware or physical safety qualification.
  ADC/pinmux, clock, DShot, gate/pad mapping and Boot/image profile separation
  remain open work.

## Clock provider split

- `system_runtime.c` now consumes a required `am13e_board_clock_start()`
  interface; it implements Rel17's 16kHz scheduling and PRIMASK gates
  without selecting an oscillator, PLL or image profile.
- `board_clock_reference.c` owns the existing 25MHz XTAL/200MHz PLL
  and clock-good readback. It still calls the original, bounded-polling
  `clock_source_reference.c` DriverLib/register implementation.
- `AM13E_MCU_RUNTIME_TICK` compiles this reusable ARM object without a
  linked reference clock; missing a real provider must remain a linker
  error on any future complete generic firmware, never a fake stub.
- Same reference MCLK, 16kHz SysTick and Rel17 flat-image checks
  and Boot compatibility gates; no physical oscillator qualification.

## Generic ADC pair acquisition split

- `analog_sampling_backend.[ch]` owns the real TI ADC reset/power, SOC pair,
  sequencer, IRQ-source setup and analog stabilization delay. All
  pinmux/physical-channel selections are explicit `AM13E_AdcPairRoute`
  values, not board defaults. The backend rejects nonconsecutive SOC
  pairs and inconsistent initialization preconditions.
- `analog_reference.h` owns only the reference ADC0 PA6/PA28 and
  ADCIN17/11 wiring. `analog_runtime.c` remains the Rel17 ISR/housekeeping
  adapter and does not invent VREF, divider or NTC curves.
- The board-neutral object compile gate is `AM13E_MCU_ADC_PAIR_BACKEND`.
  Existing calibrated/uncalibrated Rel17 firmware behavior is unchanged;
  pin/channel physical validation still requires a reviewed board.

## Host-gated MCU Fault Trip route policy

The new `fault_trip_route_plan.c` has no SDK or board dependency. The actual TI backend calls this policy before register writes and enforces SDK encoding static assertions. `fault_trip_route_plan_host_test.c` exercises 128 route pairs with mismatch/bounds rejection in CI. This is *not* proof of physical fault shutdown, electrical levels or latency.

## Command input GPIO/XBAR source split

- `command_input_route_backend.[ch]` performs real TI digital-input init and INPUTXBAR route/readback. Its descriptor requires the pin, GPIO function and XBAR route explicitly, without configuring motor outputs, pull/bias or board voltages.
- `command_input_reference.h` owns Reference PB14/GPIO46 to ECAP0 route. `command_capture.c` retains the existing exact DShot/servo callbacks, ECAP capture/IRQ/clock calibration and BiDShot dispatch.
- `AM13E_MCU_COMMAND_INPUT_ROUTE` independently ARM-compiles the generic MCU object with no Board Profile selected. Board physical pin levels, capture latency and ECAP/DMA race verification are open.

## Explicit Boot/Image Profile selection

- `profiles/image_rel17_v14.cmake` selects the approved 0x6000 Flat-APP linker, matching the native Boot and Rel17 packer. Its Flat BIN has no application header/CRC/signature, using original Cfg.id and M33 vectors.
- `AM13E_IMAGE_PROFILE=REL17_V14` is now the active full Reference build default; any other profile fails when `AM13E_ENABLE_FW1_REL17_IMAGE=ON`.
- The **object-only** generic backend path can request `-DAM13E_IMAGE_PROFILE=NONE -DAM13E_ENABLE_FW1_REL17_IMAGE=OFF`; this does not manufacture a generic runnable firmware.
- CI now verifies Reference selection succeeds and unknown image profiles are rejected, in addition to image integrity and Boot host tests.

## Six-MCPWM-pad route ownership refactor

- `motor_output_route_plan.[ch]` enforces 6-entry unique GPIO bits/PINCM and exact combined mask in native host regressions.
- `motor_output_backend.[ch]` is a TI DriverLib-based, board-neutral GPIO/PINCM implementation. It disconnects all six pads to digital input, validates actual GPIO input/peripheral-function readback, and applies/reads PWM alternate function and inversion only when called by existing qualified power-stage attach logic.
- `board_motor_output_reference.c` is the required **Reference Rel17 v1.4** provider for PA8/11/9/30/10/31. There is no implicit generic route or weak default. A different board must provide its own mapping and separate reviewed gate/Trip/RED/FED.
- `motor_safety.c` no longer directly selects PA8/11/9/30/10/31; the same default Motor MCPWM0, exclusive audio ownership and safe-off readback remain. `motor_power_stage.c` keeps PB13 inactive-first/active-last, mandatory OST/OC/Dead-band checks, and fail-closed behavior. CI gates generic ARM backend, Reference firmware/link and dedicated native invalid-pad-route regression.
- This is **not** a physical enable approval, and MCU GPIO input Hi-Z must not be equated with guaranteed external power-driver shutdown.

## IO-only auxiliary Board Configuration

Gate, driver nFAULT, independent OC, UART TX and current-sense input
are **IO-only**; no Gate/Trip/UART/current-limit behavior is linked to
the Reference FW1. RED/FED and analog numeric model are still
configured internally, with MCPWM output pads isolated. See
`BOARD_CONFIGURATION.md` for exact pin definitions and feature scope.

## Integration Design Rev1.1 alignment

Architecture implementation notes and Axx/Cxx/Bxx semantic mapping live in
`INTEGRATION_ARCHITECTURE_ALIGNMENT.md`. Native `mcu/AM13E` is the existing
AM13E23019 target directory; no duplicate framework or fake TI FOC
Application was introduced. A shared `flash_partition.h` governs
Boot/ESCape32 Config/Reserved region owners in C, plus strict Linker
Scripts. `CMD_WINDOW=6` extends Boot 1KiB addressing to all 488 logical
APP blocks; it does not silently add A/B or a second installed image.

**Rev1.4 authority:** The earlier Rev1.1 coverage mapping is superseded by
[`REL17_V14_SOURCE_GAP_AUDIT.md`](REL17_V14_SOURCE_GAP_AUDIT.md).
Original conditional source features remain in scope; their native
board/peripheral adapters are not automatically implemented.
