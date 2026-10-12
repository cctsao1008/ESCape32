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
`PWM_ENABLE` instead requires `AM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1`: each A route
is PWM, each B route is phase enable. Before the explicit pad inversion, B=1
floats, B=0/A=1 sources, and B=0/A=0 sinks. The board driver must implement that
truth table and internal complementary gate control. Its guaranteed internal
non-overlap must be supplied as `AM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS` and meet
the board minimum: MCU PWM/enable signals cannot time the driver's hidden gates.
This topology requires
equal nonzero RED/FED: A rising and B falling are delayed, rather than using
the independent-gate compare offset. Music rescales both counts. Arbitrary FED
remains unsupported. No production dead-time is inferred from the 92-tick reference.

`HALL_MAP` can select GPIO0 Hall inputs using explicit `AM13E_HALL_PIN1/2/3`
PINCM indices, `AM13E_HALL_PULL`, `AM13E_HALL_INVERT_MASK`, and
`AM13E_HALL_GPIO_TIMESTAMP=1`. EQEP0's 200 MHz unit timer timestamps GPIO ISR
entry and supplies the original 32.768 ms timeout; TIMG4/12 and ECAP0/1 retain
their owners. Hall builds retain the Core's 500 ns units and convert only at
native timer hooks. This backend does not provide a hardware edge-capture latch
or STM32 input-filter timing equivalence. IRQ latency, Hall wiring and filtering
require validation. LED/BEC sharing needs additional route ownership checks.

Rel17 requests start/stop/brake/audio through existing native hooks. The driver
seeds an inactive first image, connects real peripheral pads, releases AQ software
force, and activates PB13 after two carrier ZERO observations. Running AQ/compare/
period images transfer together through one-shot global loading. Stop/fault disables
the gate before stopping the clock and disconnecting pins. Configured PB15 trips
force both logical gate outputs LOW (PWM/enable: PWM LOW, enable HIGH) and
remain latched until watchdog reset.
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

## PA26 development command route

The current firmware selects PA26 instead of PB14 in the immutable Rev1.6
originals. Application uses PA26 PF3 / TIMG4 CCP0; Boot uses GPIO26 at
38400 baud with reset SYSOSC32MHz. `BOOT5_PB14` remains the historical build
artifact name, not an active PB14 transport. Historical `pb14` decoder/API
names are retained for protocol-independent software compatibility.

Datasheet SPRSPC3A table5-2 confirms PA26 PF3 TIMG4 CCP0, PF4 TIMG12 CCP0,
PF8/PF9 UC0 RX/TX and PF11/PF10 UC3 RX/TX. TIMG4 remains selected; Motor
TIMG12 and BEMF ECAP1 are unchanged. TRM SPRUJF2B timer allocation/table11-3
and SDK dl_timer/dl_dma define TIMG4 as 16-bit and GEN_EVENT1 DMA source34.
CC0 both-edge captures use DMA2 single transfers into a 32-word buffer;
half/completion IRQs and 16kHz SysTick drain stable prefixes. ZERO extends
timestamps without reducing the 100MHz edge resolution. Capture input
filtering is explicitly disabled; no prescale or majority filter reduces
DShot1200 edge resolution. Electrical glitch suppression remains external. CC1 is not another
RX DMA stream. RX capture requests are disabled before DMA2 becomes the
BiDShot GPIO26 toggle owner, then restored only after completion/abort.
The existing 200MHz SysTick counter measures timer reconfiguration time
and reduces the remaining reply countdown; no guessed fixed delay is used.

There is no capture FIFO or hardware overcapture counter. A detected newer
CC0 after buffer completion aborts that block, but unobserved overwrite
before DMA, bus stalls and multiple missed ZERO events cannot be ruled out
by software tests. Prefix servicing must remain within 655.36us, ZERO
service within one wrap. DShot1200 DMA reliability and physical RX/TX mux
turnaround require measurement under maximum bus/IRQ load.

A board provider can select UC0 PA26 PF8/PF9 or UC3 PF11/PF10 with
`AM13E_BOARD_SERIAL_TX_TWO_WIRE=0`, explicit existing RX/TX clock/polarity
fields, and both PINCM fields26. RX is default; TX changes the physical mux,
FIFO writes do not release the line, and actual EOT restores RX. Busy/FIFO
collision prevents starting TX; a stalled reply has bounded FIFO-flush
recovery. Native GPIO input/output inversion is explicit for SBUS/SBUS2.
Single-pad CRSF TX is rejected as full-duplex; RX-only CRSF and existing
explicit two-wire profiles remain conditional. Serial, EXBUS, CLI and SBUS2
reuse original parsers/reply policy. CLI mode0 does not steal capture until
original HIGH-timeout entry. No native simultaneous RX/TX pin function or
hardware collision detector is implied. External electrical routing,
pull-ups/level conversion and physical timings remain unqualified.
