# AM13E23019 / ESCape32 Rel17 — Software Feature Coverage Audit

**Branch:** `am13e-port-v2`. This checklist distinguishes **original Rel17 feature semantics**, **real TI MCU source**, **reference-only linked integration**, **host evidence**, and **future physical board qualification**. It is not a production approval.

## Acceptance model

| Evidence | Meaning |
| --- | --- |
| `AM13E_PORTABLE_LOGIC` | Board-neutral ARM algorithm sources compile; not an executable firmware |
| `AM13E_MCU_FAULT_TRIP_BACKEND`, `AM13E_MCU_RUNTIME_TICK`, `AM13E_MCU_ADC_PAIR_BACKEND` | Real TI device/backend objects compile without choosing reference pins, oscillator or analog external values |
| Existing Reference `AM13E_FW1_V16_IMAGE`, `BOOT5_PB14.elf` | **Reference wiring/image ABI only**; strict linked real Rel17/Boot sources; fail-closed by default |
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
| **Motor Music & PCM Audio** | `motor_audio.c` delegates exclusively to `motor_safety.c`'s **same MOTOR MCPWM0**, guarded by `audio_owner`; TIMG12 loaned for PCM cadence | Music/PCM arithmetic host test + linked source | Verify physical MCPWM output/audio behavior only when safe; **NO GPIO buzzer** |
| DShot RX / PWM servo / command scheduling | Generic `command_input_route_backend.c`, Reference `command_input_reference.h` / `pb14_capture.c`, `src/io.c` | Generic ARM GPIO/XBAR object, capture/CRC/abort host and strict link | Separate reference ECAP IRQ/BiDShot routing, verify capture latency and physical input |
| BiDShot TX / GCR-NRZI | `bidir_codec.c`, `bidir_timing.c`, `pb14_bidir_tx.c` | Exhaustive payload host tests + strict link | Generic bidirectional pad route; DMA/collision and turnaround qualification |
| Extended DShot / telemetry mode policy | `src/telem.c`, `telem_mode_plan.c` | Telemetry modes host test + FW1 link | Optional actual UART/CAN transport profiles; do not claim DroneCAN complete |
| BEMF zero-cross / timeout | `motor_bemf.c`, ECAP clock and timeout plans | Host BEMF clock/timeout + full FW1 | Comparator input board profile, timing/noise/overrun qualification |
| Fault nFAULT asynchronous hardware trip | `fault_trip_backend.c` + `fault_trip_route_plan.c`; Reference route in `motor_nfault_trip.c` | 128-route host regression + independent ARM object + FW1 link | Physical active-level and shutdown-path verification |
| Independent OC / Dead-band / Gate outputs | `motor_power_stage.c`, `motor_safety.c` | Synthetic compile branch gate ONLY, **never flashed** | Reviewed electrical profile; **gate fail-closed remains default** |
| ADC raw monitoring / optional scaling | `adc_pair_backend.c`; Reference pin/channels in `adc_board_reference.h` and ISR in `adc_runtime.c` | Native scaling host + independent ARM object + FW1 link | Board VREF, divider, thermistor and channel route, current sensing not assumed |
| Clock / Rel17 tick / neutral arming window | `system_runtime.c`, `runtime_tick_contract.h`; `board_clock_reference.c` for 25 MHz/200 MHz only | ARM generic runtime object, linked Reference Clock Diagnostic, 250 ms arming host test | Alternative real clock providers with documented physical oscillator |
| Flash config persistence / WWDT / update transaction | `cfg_flash_*.c`, Boot sources, image tool | Flash planning/writer host, 6 Boot host tests and 16 image checks | Distinct target-specific Boot/Image Profile selection and transport capacity |
| Resource ownership / safety | `motor_safety.c`, `motor_runtime_irq.c`, `motor_power_stage.c` | Strict link and physical-output compile branch; source review | Dedicated negative-path ownership and runtime integration tests; physical qualification last |

## Constraints that must remain invariant

- **No input command can implicitly enable the power stage.** Unknown gate polarity, external OC, RED/FED or calibration means no output. Synthetic CI electrical settings are compilation fixtures, never production defaults.
- **Motor and Audio share MCPWM0**, and ownership is exclusive. Do not add a standalone GPIO beeper path.
- Current Reference Image: `APP_BASE=0x6000` and existing single-slot 1KiB-block/8-bit-index update transport. **Preserve magic, CRC, signature, vector, and existing Boot/Flash semantics.** A different profile would need its own explicit ABI/version/migration validation; changing an output filename is not an ABI migration.
- Host/CI PASS is source-level evidence, not AM13E silicon timing, a gate-drive clearance, or a complete generic board-independent FW1.

## Remaining source-port backlog (not yet PASS)

1. Extract MCU motor pad mux, gate driver, nFAULT and independent OC into explicit **optional Board Profile**, keeping original electrical checks. Continue replacing Reference-only hardwired routes in FW1 without relaxing fail-closed behavior.
2. Separate PB14 ECAP/DShot/BiDShot reference route into an input-signal board provider and generic capture/timing backend; preserve exact Rel17 command and telemetry semantics.
3. Separate Boot/Image selection from silicon backend with **distinct explicit profile**; keep existing Reference linker, `0x6000`, packer ABI, CRC and signature untouched.
4. Expand software negative-path/integration regression for resource ownership, faults, update rejection, and profile misconfiguration. Qualify reference image and alternative profile independently.
5. After **all firmware source/refactoring and software regression/coverage gates pass**, request board schematics, review electrical parameters and only then schedule physical tests.
