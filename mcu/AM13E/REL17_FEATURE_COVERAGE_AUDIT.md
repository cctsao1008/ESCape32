# AM13E23019 / ESCape32 Rel17 — Software Feature Coverage Audit

**Branch:** `am13e-port-v2`. This checklist distinguishes **original Rel17 feature semantics**, **real TI MCU source**, **reference-only linked integration**, **host evidence**, and **future physical board qualification**. It is not a production approval.

## Acceptance model

| Evidence | Meaning |
| --- | --- |
| `AM13E_PORTABLE_LOGIC` | Board-neutral ARM algorithm sources compile; not an executable firmware |
| `AM13E_MCU_FAULT_TRIP_BACKEND`, `AM13E_MCU_RUNTIME_TICK`, `AM13E_MCU_ADC_PAIR_BACKEND` | Real TI device/backend objects compile without choosing reference pins, oscillator or analog external values |
| Existing Reference `AM13E_FW1_V16_IMAGE`, `BOOT5_PB14.elf` | **Reference wiring/image ABI only**; strict linked real Rel17/Boot sources; motor PWM logic active, physical Gate/OC/nFAULT IO-only |
| Native Host regressions | Control arithmetic, encoding and negative cases; no evidence of gate-driver, current-sense or capture latency |
| Image `verify_v16_image.py` + Boot host CTest | Reference vector, linker layout, metadata, signature-last update transaction, CRC and Flash range compatibility |
| Future hardware gate | Physical electrical pinmux, protection latency, output polarity, calibration and motor drive are **not** approved |

## Firmware feature coverage (software scope)

| Rel17 feature / contract | Source owner / reuse boundary | CI software evidence | Remaining generic-platform work |
| --- | --- | --- | --- |
| Original Rel17 main/arming/protection policy | `src/main.c`, `src/io.c`, `src/util.c`, `src/telem.c`, `src/prog.c` | Full Reference FW1 object compile + strict ELF link | Board-neutral application linking without Reference BSP |
| Six-step / CW-CCW / drag/lock braking AQ | `motor_phase_plan.c`, `motor_aq_plan.c`, `motor_safety.c` | Host phase, motor AQ, brake, lock + Reference strict link | Generic pad routing; on-board switching validation |
| Duty clamp, PWM frequency/ramp, shadow and timer arithmetic | `motor_duty_plan.c`, `motor_frequency_plan.c`, `motor_pwm_shadow_plan.c`, `motor_shadow_plan.c`, `motor_timer_math.c` | Dedicated host tests plus ARM portable objects | Confirm actual PWM/timebase waveform before gates |
| Sine startup and modulation carrier | `motor_sine_table.c`, `motor_safety.c` | Sine host test + Reference link | Board comparator / commutation timing verification |
| **Motor Music & PCM Audio** | `motor_audio.c`, `motor_ownership_plan.c` and real `motor_safety.c` transitions share **the same MCPWM0** and TIMG12 PCM cadence | 48 ownership matrix transitions incl. Six-step/Sine/Brake/Music/PCM, negative overlaps, Music/PCM numerical regressions and FW1 link | Physical audio/motor pads remain isolated; **NO GPIO buzzer** |
| DShot RX / PWM servo / command scheduling | Generic `command_input_route_backend.c`, Reference `command_input_reference.h` / `command_capture.c`, `src/io.c` | New ACK-epoch snapshot and complete four-event flag/race rejection; DShot150/300/600/CRC roundtrip native tests and strict FW1 link | Measure ECAP0 ISR execution and full-group capture overwrite on silicon; optional DMA capture unimplemented |
| BiDShot TX / GCR-NRZI | `bidir_codec.c`, `bidir_timing.c`, `command_reply.c` | Exhaustive 4096 payload host tests, final-edge 30us / rollover / rejection tests, full DShot150/300/600 decoder→CRC→reply-plan host and strict link | IRQ/DMA latency, physical turnaround and external contention not verified |
| Extended DShot / telemetry mode policy | `src/telem.c`, `telem_mode_plan.c` | Telemetry modes host test + FW1 link | Optional actual UART/CAN transport profiles; do not claim DroneCAN complete |
| BEMF zero-cross / timeout | `motor_bemf.c`, `motor_bemf_event_plan.c`, original `src/main.c`, `motor_event_timer.c` | Shared ECAP1 edge/timeout and original policy planner compiled into runtime; 6-step × 2 direction × 2 damp, early/reject/timeouts and TIMG12 native integration test; strict FW1 link | Analog comparator latency/noise and actual commutation jitter not verified |
| Driver nFAULT GPIO (IO-only) | `fault_input.c` initializes PB15 input; `motor_fault_route.c` is **not** linked in FW1 | PB15 inactive interrupt compile and IO-only contract; generic trip backend ARM object/route host tests retained | GPIO ISR and OST1 Trip intentionally NOT IMPLEMENTED |
| PWM/Dead-band plus Gate IO-only | `motor_safety.c` compiles MCPWM/RED/FED algorithms; `motor_power_stage.c` initializes PB13 INACTIVE and never attaches | Full FW1 strict link and IO-only source/compile regression | No physical gate enable, six output pins remain isolated; independent OC/OST2 not implemented |
| ADC raw monitoring / default model | `analog_sampling_backend.c`; Reference pin/channels in `analog_reference.h` and ISR in `analog_runtime.c` | G431-derived numeric model feeds Rel17 adcdata; scaling native host + full FW1 link | Actual VREF, divider, NTC, current-sense hardware require board work |
| Clock / Rel17 tick / neutral arming window | `system_runtime.c`, `runtime_tick_contract.h`; `board_clock_reference.c` for 25 MHz/200 MHz only | ARM generic runtime object, linked Reference Clock Diagnostic, 250 ms arming host test | Alternative real clock providers with documented physical oscillator |
| Flash config persistence / WWDT / update transaction | `cfg_flash_*.c`, Boot sources, image tool | Flash planning/writer host, 6 Boot host tests and 16 image checks | Distinct target-specific Boot/Image Profile selection and transport capacity |
| Resource ownership / safety | Generic `motor_output_backend.c` and `motor_output_route_plan.c`; Reference `board_motor_output_reference.c`; `motor_safety.c` / `motor_power_stage.c` | Independent ARM object, pad-route negative Host Test, Reference strict link and physical-output compile | Additional runtime ownership/fault integration tests; qualified hardware only last |

## Constraints that must remain invariant

- **Five IO-only exclusions**: PB13 Gate Enable, PB15 driver nFAULT, independent OC Trip, Serial Telemetry TX and current limiting are **pin-mode initialized only** (the last three are unassigned until a real pin is selected). No Gate activation, OST1/OST2, Serial TX or current-limit algorithm is connected in FW1. PWM/RED/FED remains internally operational with phase pins isolated. This is not a motor-spinning configuration.
- **Motor and Audio share MCPWM0**, and ownership is exclusive. Do not add a standalone GPIO beeper path.
- Current Reference Image: `APP_BASE=0x6000` and existing single-slot 1KiB-block/8-bit-index update transport. **Preserve magic, CRC, signature, vector, and existing Boot/Flash semantics.** A different profile would need its own explicit ABI/version/migration validation; changing an output filename is not an ABI migration.
- Host/CI PASS is source-level evidence, not AM13E silicon timing, a gate-drive clearance, or a complete generic board-independent FW1.

## Remaining source-port backlog (not yet PASS)

1. Extract MCU motor pad mux, gate driver, nFAULT and independent OC into explicit **optional Board Profile**, keeping original electrical checks. Continue replacing Reference-only hardwired routes in FW1 without relaxing fail-closed behavior.
2. Separate PB14 ECAP/DShot/BiDShot reference route into an input-signal board provider and generic capture/timing backend; preserve exact Rel17 command and telemetry semantics.
3. Separate Boot/Image selection from silicon backend with **distinct explicit profile**; keep existing Reference linker, `0x6000`, packer ABI, CRC and signature untouched.
4. Expand software negative-path/integration regression for resource ownership, faults, update rejection, and profile misconfiguration. Qualify reference image and alternative profile independently.
5. After **all firmware source/refactoring and software regression/coverage gates pass**, request board schematics, review electrical parameters and only then schedule physical tests.

## Boot/Image profile audit result

A separated CMake `REFERENCE_V16` image selector now binds the existing APP linker and packing tools, with reject-by-default behavior for unimplemented image profiles. This is a **build contract only**, not a second image ABI, and it does not remove the Reference Board wiring from the present full FW1. The current image magic, CRC, signature and metadata remain untouched. Source work for a true independent board/image variant is still open.

## Automated E2 coverage evidence gate (CI)

`REL17_FUNCTIONAL_COVERAGE.json` is a machine-readable manifest for
**ported-software**, **five explicit IO-only**, and **outside-FW1**
features. `tools/verify_rel17_coverage.py` runs after REAL ARM object
compile/strict FW1 link and Boot v1.6 host transaction; it requires:

1. Every declared live feature source actually compiled in the
   `CMakeFiles/AM13E.dir/` application object list.
2. Each mapped Native Regression reported an actual `PASS` in
   `host-tests.log` (not merely present in a filename).
3. ECAP0 ACK-epoch snapshot, 30us BiDShot planner, ECAP1
   candidate classification/Rel17 advance and Motor/Audio ownership
   are **called from real linked Runtime paths**.
4. PB13 remains inactive-output-only, PB15 interrupt and OST1/OST2
   remain unlinked, no Current Limit/Serial TX activation or invented
   optional IO pin is introduced.
5. Reference FW1 and Boot v1.6 ELFs, packed-image output, strict
   link and both Boot host test results are present and PASS.

CI uploads generated `am13e-ci-logs/rel17-functional-coverage.json`
and `rel17-functional-coverage.md` with precise feature-to-source-to-test
traceability. They are software evidence, not MCU on-target
instrumentation. They deliberately list pending physical validations
including DShot600 capture overwrite/latency, BiDShot 30us waveform,
CMPSS BEMF/noise, PWM pad/gate physical outputs (disconnected by design),
sensor calibration, WWDT reset timing and Flash power-failure behavior.

**Completion wording:** Tasks 1–4 are source/Host/ARM CI delivery gates.
Do not interpret them as electrical qualification or as implementation of
the user-excluded IO-only features.
