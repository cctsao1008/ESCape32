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
| Image/update profile | Flash regions, boot entry/VTOR, CRC/signature protocol, delivery transport | Assuming every AM13E board shares a boot partition |
| Application | ESCape32 Rel17 policy, input/arming, protection, telemetry and music | Claiming hardware-verified behavior from host tests |

## Current build targets

- `AM13E_PORTABLE_LOGIC`: a **board-neutral ARM object compile gate**.
  It compiles code that takes explicit values or produces control plans;
  no board I/O is configured. It is not an ELF to flash.
- `AM13E_FW1_V16_IMAGE` and `BOOT5_PB14.elf`: **existing reference
  integration**. These are real linked images but they encode a fixed
  0x6000 APP location and fixed reference pin assignments. They must
  not be described as a generic AM13E board support package.
- The reference power stage deliberately remains fail-closed until a
  separately reviewed electrical configuration is provided.

## First generic build gate

```bash
cd ~/github/ESCape32
cmake -B build-am13e-v16 -D AM13E_SDK_ROOT="$HOME/ti/am13e230x_sdk_26_01_00_03"
cmake --build build-am13e-v16 --target AM13E_PORTABLE_LOGIC --parallel 4
```

### Next source refactoring work

1. Introduce an independently selectable board descriptor for input
   routes, sensing, and motor outputs; avoid treating PB14, PB13, and
   PA6/PA28 as universal device defaults.
2. Decouple CPU/peripheral init from the reference crystal and timer
   topology, keeping concrete DriverLib register checks.
3. Isolate the Boot/APP linker layout and image metadata behind an
   explicit image profile; **do not change existing on-flash magic, CRC
   or upgrade compatibility as a mere naming exercise**.
4. Move reference pin assignment behind an opt-in BSP and establish
   a board-free automated build/test gate.
5. Keep Motor Drive and Motor Audio on the same MCPWM resource with
   exclusive ownership; neither is a standalone GPIO buzzer.

This document is a porting boundary, not a hardware qualification.

## MCU-side fault trip source separation

- `fault_trip_backend.[ch]` is a **board-neutral TI DriverLib backend** with
  explicit MCPWM/XBAR route parameters, route consistency checks and actual
  register/status readback. It never selects pins, arms outputs or clears OST.
- `motor_nfault_trip.c` is the **Reference Board adapter** choosing PB15,
  INPUTXBAR2, PWMXBAR1, active-low and MCPWM0 OST1. Fault latching,
  readback and fail-closed error handling remain mandatory.
- `AM13E_MCU_FAULT_TRIP_BACKEND` is an independent board-free ARM object
  compile gate. CI also compiles the full reference firmware, uses ESCape32
  CMake + Unix Makefiles, and now treats host regression failures as fatal.
- This is not a universal runnable firmware or physical safety qualification.
  ADC/pinmux, clock, DShot, gate/pad mapping and Boot/image profile separation
  remain open work.
