# Native AM13E build

The unified upstream `add_target(name mcu ...)` builds one Application and one Boot image. Each native config.cmake selects its own Core/HAL sources; private cmake/ti.cmake supplies official SDK integration, Cortex-M33 ABI, startup and linker requirements. No image-profile modes or firmware aliases remain. Application/Boot HAL objects are separate. Shared runtime and serial helpers are explicitly selected from src/runtime and src/serial; legacy src/*.c selection remains the original five files.

Use official Arm 15.2.Rel1/GCC 15.2.1 and TI SDK 26.01.00.03.STS. Set AM13E_SDK_ROOT, ESCAPE32_BUILD_AM13E=ON and CMAKE_C_STANDARD=23. Build targets AM13E and BOOT5_PB14. Normal outputs are AM13E-rev17.3.{elf,bin,hex} and boot/BOOT5_PB14-rev4.{elf,bin,hex}. The Application BIN is the actual linked image with at most three erased transport padding bytes. Its JSON sidecar is build evidence only, never a mandatory wire manifest. Boot banks and Application ABI use each image's native config.ld.

Legacy builds need only libopencm3; ESCAPE32_BUILD_AM13E=OFF removes TI SDK/Python requirements. With AM13E_SDK_ROOT supplied, native images are enabled by default. No AM13E st-flash target is created. AM13E_BUILD_DIAGNOSTICS=ON exposes only opt-in peripheral compile gates, excluded from normal builds. Clock/startup/RAMFUNC evidence is taken from the real firmware images instead of a duplicate clock executable.

Run boot/tests/am13e_host with AM13E_HOST_PACKED_IMAGE pointing to the real Application BIN and all 11 CTests. Run mcu/_support/AM13E/tools/run_host_regressions.sh full for existing native tests; fast selects development groups. Protocol/Core fixtures are in src/tests/AM13E, hardware-only units in mcu/AM13E/hal/tests, Boot tests in boot/tests. They never enter production source lists. CI additionally links selected serial/telemetry and conditional GPIO profiles, rejects unsafe recovery combinations, and audits image layout, actual Core integration and original design integrity.

Rev1.6 alone governs current integration. The three originals in v1.6 remain byte-identical; superseded documents, crosswalks, manifest experiments and duplicate firmware/linker fixtures are retired in Git history. REV16_REQUIREMENTS.csv and REL17_FUNCTIONAL_COVERAGE.json retain current gap/evidence coverage, not acceptance claims.

Config-prefix wire blocks0–3 map4000–4fff; APP blocks4–491 map6000–7ffff;492–511 are invalid and Reserved5000–5fff is never mapped. APP-only uploads start at4; Config+APP streams require explicit Config input/authorization. Real parser/range/writer tests retain short-write, CRC, sector companion preservation, failure/retry, protection and readback coverage.

Reference pin/clock/gate values remain unqualified. Physical motor and Flash operations, Bank0 self-update and NONMAIN programming are prohibited during software review. Existing recovery and fail-closed protection gates remain mandatory. Static WRP conservatively rejects protected banks; exact reversible permissions need TI/silicon resolution. SBUS2/soft-UART timing, loaded IRQ equivalence, receiver discovery/reselection, Hall routing and board electrical/resource topology remain pending. Build/mock PASS is neither complete Rel17 parity nor hardware qualification.

## Board-defined physical motor outputs

The existing board-provider profile supplies `AM13E_MOTOR_GATE_ACTIVE_LEVEL`,
`AM13E_MOTOR_PAD_INVERT_MASK` (UH/UL/VH/VL/WH/WL bits 0–5),
`AM13E_MOTOR_OUTPUT_RED_TICKS`, `AM13E_MOTOR_OUTPUT_FED_TICKS`,
`AM13E_MOTOR_MIN_DEAD_TICKS`, `AM13E_MOTOR_NFAULT_ACTIVE_LOW` and
`AM13E_MOTOR_GATE_RESET_INACTIVE`. Partial/invalid definitions fail compilation.
These describe electrical wiring, not another application arming policy.
No complete production-board definition is currently supplied.

The six-independent-input adapter preserves Core phase selection using separate
A/B AQ sources. RED delays A rising; FED is zero; compare-delayed B rising
provides the opposite switching gap. RED must meet the explicitly defined minimum
at the motor/PCM 100 MHz TBCLK. Music rescales RED upwards to whole 6.25 MHz ticks.
Nonzero FED and `PWM_ENABLE` gate-driver topologies need a separate electrical
adapter and are rejected rather than guessed. No production dead-time is inferred
from the provisional 92-tick reference.

Rel17 requests start/stop/brake/audio through existing native hooks. The driver
seeds an inactive first image, connects real peripheral pads, releases AQ software
force, and activates PB13 after two carrier ZERO observations. Running AQ/compare/
period images transfer together through one-shot global loading. Stop/fault disables
the gate before stopping the clock and disconnecting pins. Configured PB15 trips
force both logical gate outputs LOW and remain latched until watchdog reset.
PB15 electrical routing, external reset-inactive gate wiring, pin inversion and
independent overcurrent protection still need board signoff.

With `AM13E_SDK_ROOT` set, the existing full native runner additionally executes
actual Core `nextstep()` and native HAL with official TI register definitions in
anonymous host RAM, using synthetic electrical fixtures with normal, inverted-high, full-duty and software-blanking configurations. These fixtures are
never production-board parameters. Observe all six physical pins with the power
stage safely isolated before qualifying polarity, duty/frequency, dead time,
commutation transitions or asynchronous trip timing; register-model PASS does not
establish physical waveforms or power-stage safety.

SW_BLANKING uses MCPWM0 CMPC (610 motor-TBCLK ticks, the existing G431
1024/168 MHz reference rounded up), ET1 ZERO shared with startup, and ET2 CMPC.
It gates ECAP1 timestamp capture in the original slowest ERPM band without
resetting elapsed BEMF time. IRQ latency and analog blanking effectiveness
remain unmeasured. Music temporarily loans ET2 for a sticky period event;
its IRQ masks itself until the original polling callback consumes that flag.
INVERTED_HIGH requires the explicit board pad mask to invert UH/VH/WH;
it never silently overrides a conflicting electrical board definition.
Logical FED=0 bypasses the B delay path, with unused DBFED/DBFEDS kept at 1
because TI forbids loading zero dead-band counts at global ZERO.
