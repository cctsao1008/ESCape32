# AM13E Rel17 FW1 — Missing Backend Implementation Pseudocode

> **Corrected product I/O scope:** FW1 external command/telemetry is **PB14-only**: PWM/DShot receive, BiDShot and Extended DShot telemetry. No UART/UNICOMM2 is used by ESCape32 FW1. The historical UART symbols listed below are **not FW1 requirements**. See [FW1_PB14_ONLY_SCOPE.md](FW1_PB14_ONLY_SCOPE.md). Do not remove `src/telem.c::sendtelem()`: its Extended DShot scheduler remains active with UART transport excluded. `--no-undefined` stays mandatory for real remaining hardware backends.


**Status:** Design / TODO; intentionally **NOT compiled**; no MCU callback stubs or fake return values.
**Source of missing-symbol evidence:** E1-AO `e1ao-fw1-link.log` (22 distinct unresolved symbols / 32 references); incremental ARM GNU Object Compile PASS with zero warning for `cfg_flash_plan.c`. E1-AP adds the testable Flash write engine but has not yet passed its own ARM/Host rebuild.
**Reference boundaries:** Original ESCape32 Rel17 `src/main.c`, `src/io.c`, `src/telem.c`, `src/util.c`, `src/prog.c`; AM13E target-specific contracts in `mcu/AM13E/*.h`; AM13E230x TI TRM/SDK; project SW/HW architecture baseline v1.6.

## E1-AN evidence and E1-AO configuration Flash preparation (2026-10-10)

- **E1-AN measured:** ARM GNU Object Build PASS. No application-owned C warning. TI SDK 26.01.00.03 `dl_timer.c` reports **10 `-Wmaybe-uninitialized` warnings**, because this run recompiles vendor Capture/Compare helpers. FW1 Strict Link FAIL: **22 unique Undefined Symbols / 32 references**, down from E1-AL's 27/38. All five UART symbols are absent. These counts are from the uploaded `e1an-{arm-compile,fw1-link}.log`, not estimates.
- **FW1 I/O boundary:** PB14 RX/BiDShot/Extended DShot only. Never reintroduce UC2/PA22/PA23 UART to silence missing symbols.
- **E1-AO new tested helper:** `cfg_flash_plan.{c,h}` checks write destination is **exactly 0x4000**, the FW1 settings partition spans only `0x4000..0x4fff`, source is entirely SRAM_S `0x20000000..0x20017fff`, Flash sector size 2048 bytes, and ECC program-unit padding is a multiple of 16 bytes. Local host GCC `-O2 -Wall -Wextra -Werror -pedantic` **PASS for all 4096 sizes** and invalid address/overflow cases. Physical AM13E ARM GNU and Flash execution are NOT covered by this host test.
- **Do not implement** `am13e_app_cfg_commit` as an ACK-only wrapper. Before any real erase/program, verify RAM execution of the entire FlashCTL call path (including interrupt/exception safety), FW1/FW2 partition boundaries, active-bank handling, ECC tail staging, read-back and power-loss recovery. The historical `linker_app_reference.ld` is not a production image map; retain `--no-undefined` until an actual Flash backend exists.
- **Outstanding** (measured E1-AN): 14 Motor/Commutation/BEMF, 7 Audio, 1 Configuration Flash. The E1-AO planner is not itself a resolved backend symbol.

## E1-AP — Real Flash transaction sequence, *without* unsafe erase enable

- **E1-AO measured:** ARM GNU incremental object compile PASS, 0 warnings in `cfg_flash_plan.c`; FW1 strict link still FAIL with **22 unique Undefined Symbols / 32 references**. A single-object incremental build does not re-evaluate TI `dl_timer.c` warnings.
- E1-AP introduces `cfg_flash_writer.{c,h}`: executable, hardware-independent erase / 16-byte ECC word program / read-back sequence, explicit failure propagation, SRAM word alignment and 0xFF pad. Test fixture simulates the two FW1 sectors and verifies **FW2 0x5000–0x5fff is never modified**; injected erase, program and read/compare failures return failure.
- **Not yet integrated into physical Flash:** `am13e_app_cfg_commit` intentionally remains an unresolved symbol. TI `DL_Flash_eraseSector`, `DL_Flash_program` and `DL_FlashCTL_executeCommand` are identified as hardware adapter candidates; only the command polling is declared `RAMFUNC` in the SDK. Before any irreversible erase, prove CPU/IRQ/NMI Flash-bank execution safety, WWDT timing (a valid DShot input has already armed WWDT), actual linker RAMFUNC copy, and power-interruption behavior.
- Existing `main()` reads the live configuration from fixed Flash `0x4000`. Therefore this writer **does not guarantee power-fail atomicity** even though every completed write is read-verified. Require a versioned 2-sector journal/validity selection or explicit agreed loss-of-settings recovery policy before production activation.
- PB14-only configuration is unchanged. No UART SDK, no no-op link workaround, no relaxation of `-Wl,--no-undefined`.

## Current implementation ledger — after E1-AB through E1-AE

**Build evidence:** E1-Z had 32 symbols / 44 references. E1-AJ ARM GNU Object Build passed with zero warnings; the strict FW1 Link reports **27 unique Undefined Symbols / 38 references**, one symbol/two references fewer than E1-AI. This is measured Link evidence, not WWDT0/Motor silicon validation.

| Backend function / hardware resource | Current source state | Verification remaining |
|---|---|---|
| `am13e_app_motor_init` | **SOURCE IMPLEMENTED (inactive preflight only)** in `motor_safety.c`: real MCPWM0 SDK init; counter Stop/Freeze, six AQ outputs low, PA8/PA11/PA9/PA30/PA10/PA31 remain GPIO Input/Hi-Z, register readback | ARM GNU, TRM register timing, board pad state, external gate behavior |
| `am13e_app_motor_fault_shutdown` | **SOURCE IMPLEMENTED (MCU-side shutdown fallback)**: latch fault, stop MCPWM timebase and disconnect MCU PWM pads; does not assert any unverified PB13 gate polarity | Prove actual driver shutdown and hardware comparator-to-MCPWM bounded trip |
| `am13e_app_motor_fault_reset` | **SOURCE IMPLEMENTED (non-returning fault hold)**, pending board-safe reset release conditions | Physical fault and post-reset behavior |
| `MCPWM0_IRQHandler` | Strong SDK startup vector, read Trip Zone/interrupt flags, ACK, enter latched fault path; **does not implement/enable the physical over-current Trip Source** | IRQ vector/map check; fault injection, latency, and hardware trip independent of CPU |
| `am13e_app_motor_sine_schedule_us` | **SOURCE IMPLEMENTED**: TIMG12 100MHz BUSCLK one-shot schedule, real TIMER IRQ to unchanged Rel17 `nextstep` callback | ARM GNU, scope timing/jitter and priority tests |
| `am13e_app_motor_bemf_commutation_delay_us` | **SOURCE IMPLEMENTED** using the same TIMG12 one-shot; previously not visible in the 32-symbol link baseline | IRQ dispatch from physical BEMF zero-cross, timing proof |
| Rel17 six-step `p/n/cc` decoding | **SOURCE IMPLEMENTED** as pure-C `motor_phase_plan.{c,h}` with the **unmasked** six original `p/n` values. `cc` is comparator code, not a float-phase mask | Host Test, later real MCPWM action-qualifier adaptation and scope |
| Motor microsecond conversion | Pure-C `motor_timer_math.{c,h}` with overflow checks | Host Test and peripheral clock measurement |
| Rel17 PWM frequency interpolation | E1-AH pure-C `motor_frequency_plan.{c,h}`: 1000–2000us **period** interpolation; `ertm_us=0` uses minimum frequency. ARM GNU PASS. E1-AI corrected its Host Test's 1GHz/16kHz valid 62500-tick boundary | Host Test and motor scope; final MCPWM driver remains unresolved |
| Rel17 duty compare policy | E1-AI pure-C `motor_duty_plan.{c,h}` models `scale()` with `running`, `lock`, `damp`, `brushed`, `FULL_DUTY`, and supplied board dead-time ticks; the AM13E-only call interface now includes `running` | Host Test, ARM GNU; actual dead-time and PWM compare hardware still unresolved |
| Real receiver-input WWDT0 | E1-AJ `input_watchdog.c`: WWDT0 powered during PB14 `initio()`; starts after first validated PWM/DShot command; zero closed window, 2^15 nominal LFCLK ticks; `BOOTWWDT0` maps to FORCE_ARM. | **ARM GNU PASS**; verify clock, actual reset latency, Boot ownership and command-loss behavior on hardware |
| Telemetry mode mapping | E1-AK/E1-AL pure-C `telem_mode_plan.{c,h}` preserves baud/RX behavior, legacy **HDSEL single-wire on all modes**, iBUS/S.Port/MSB/HoTT RX→reply ownership, S.Port inversion and `RTOR=26`. E1-AL explicitly rejects 26 when mapping to TI UART RXTOSEL's **0–15** field; a separate frame-gap mechanism is required. | E1-AK ARM GNU PASS; E1-AL Host/ARM pending. Must qualify UC2 PA22/PA23 single-wire wiring, S.Port external inversion, UART RX gap timing and completion before claiming transport parity |

**Updated FW1 build boundary (PB14-only):** E1-AN excludes legacy `inittelem()`, UART protocol handlers, `sendtelemdata()` and UC2 SDK DriverLib objects from the FW1 compile graph. It **keeps the original `sendtelem()` Extended DShot 32ms scheduler** and PB14 GCR/NRZI TX. The five historical `am13e_telem_hw_*` undefined symbols should no longer be required. The **last measured** count remains E1-AL's 27/38 until a new WSL compile/link run; an estimated improvement is not a measured linker result. Optional UART prototyping code remains in the tree but is not linked into FW1.

**Still intentionally unresolved:** `am13e_app_motor_runtime_enable_interrupts` and `am13e_app_motor_commutation_enable` cannot legitimately release the inverter before the PB13 enable polarity, real hardware over-current source → PWMXBAR → MCPWM Trip Zone path, inactive fault action, timer/dead-time policy and relevant board protection checks are verified. This is a real firmware safety dependency, not removal of any Rel17 feature. `sine_write`, `sixstep_write`, `pwm_apply`, comparator/BEMF configuration, the watchdog, audio and config-flash driver also remain unimplemented. UART is outside FW1 scope.

**Required build gates:** `cmake --build build-am13e --target AM13E -j"$(nproc)"`, and separately `cmake --build build-am13e --target AM13E_FW1.elf -j"$(nproc)"`. The latter must continue to use `-Wl,--no-undefined`. The known `linker_app_reference.ld` is a historical Boot-v2 linker smoke contract, **not** a qualified production image linker map. All motor outputs must remain inactive until production HW/Flash/vector contracts are reconciled.

**Host tests added:** `tests/motor_phase_plan_host_test.c`, `tests/motor_timer_math_host_test.c`, `tests/motor_frequency_plan_host_test.c`, and E1-AI `tests/motor_duty_plan_host_test.c`. Their results are pending WSL execution; none tests MCU GPIO/MCPWM/TIMG12 silicon behavior.

## Contract and integration rules

- This document records the **E1-Z missing-symbol baseline**; some entries now have real source implementations as tracked in the current ledger above. Never add `return 1;`, empty `void` bodies, or dummy callback handlers to satisfy `-Wl,--no-undefined`.
- Keep original Rel17 application as the only policy owner, and all AM13E register/clock/interrupt/pin handling in `mcu/AM13E/`. This document is a *plan*, not an alternate firmware.
- Keep exact existing prototypes in `motor_backend.h`, `io_backend.h`, `telem_backend.h` and `util_backend.h`; `compctl(int)` is declared in `src/common.h`.
- **Motor bridge safety:** MCPWM0 outputs PA8/PA11, PA9/PA30, PA10/PA31; PB13 GPIO45 gate enable (electrical polarity NOT yet qualified); PB15 GPIO47 nFAULT active-low backup input; HW comparator → PWMXBAR → MCPWM trip is mandatory and not replaced by PB15 software IRQ or SysTick polling.
- BEMF uses CMPSS0 COMPH PA17/PA4, CMPSS1 COMPH PA3/PA2, CMPSS3 COMPH PA16/PA18 per HW v1.6. Do not assume these BEMF comparators are automatically the *over-current* trip source; select/qualify the actual protection comparator, threshold, XBAR source and shutdown polarity separately.
- The 25MHz external crystal / nominal 200MHz MCLK is already handled by the existing clock backend. Use the actual peripheral clock and verified timer counts, not nominal CPU cycles guessed for individual modules.
- FW1 configuration region is 0x00004000–0x00004fff, FW2 settings are 0x00005000–0x00005fff, and the application is 0x00006000–0x0007ffff. Boot is frozen; preserve image/flash contracts.
- Do not remove DShot RX, BiDShot TX, **Extended DShot telemetry over PB14**, music/audio or config persistence from FW1 functional scope. UART telemetry is explicitly out of FW1's product requirements. Some items are source-integrated and others remain unverified; implementation and hardware qualification are distinct milestones.
- **No SDK board example wiring is authoritative for the product.** Confirm selected MCU peripheral mux, interrupts, DMA channels, output polarities and system-level protection on the board.

## Missing-symbol register (32 symbols / 44 references)

| Area | Count | Dependency |
|---|---:|---|
| Motor / MCPWM / BEMF / Protection | 18 | MCPWM0, CMPSS, PWMXBAR, interrupts, gate driver and Trip Zone |
| Command input watchdog | 1 | Valid-frame supervision / timeout reaction |
| Historical UART telemetry (excluded from AM13E FW1) | 5 | PB14-only FW1 does not use UART; Rel17 UART sources retained for other targets |
| Audio / motor-powered tones | 7 | Rel17 music/PCM timing, MCPWM ownership |
| FW1 configuration flash | 1 | Flash sector/ECC, RAMFUNC, partition verification |
| **Historical E1-Z total** | **32** | PB14-only FW1 scope correction pending next WSL Link; `--no-undefined` retained |

## Motor / MCPWM / BEMF / Protection (18)

Existing contract: `mcu/AM13E/motor_backend.h`.

1. `am13e_app_motor_init` → proposed `motor_power.c`

   ```text
   configure six MCPWM0 outputs as inactive; configure pin mux, complementary actions, period/dead-band, fault inputs; configure CMPSS0/1/3 BEMF and timestamp routing; establish hardware trip path; verify fault latched and PB15 nFAULT healthy BEFORE any bridge-enable transition; record ready state
   ```

2. `am13e_app_motor_runtime_enable_interrupts` → proposed `motor_power.c`

   ```text
   validate VTOR, implemented IRQ vectors, proper priorities, clock, PB13 gate polarity, hardware trip, and initial inactive outputs; only after checks enable required IRQs and clear PRIMASK; fail safely on any check failure
   ```

3. `am13e_app_motor_pwm_apply` → proposed `motor_pwm.c`

   ```text
   clamp Rel17 logical duty (0..2000); derive frequency from freq_min/freq_max plus ertm; apply duty, damping, lock and brushed semantics; stage shadow compare and PWM-period update, avoiding spurious complementary pulses
   ```

4. `am13e_app_motor_sine_schedule_us` → proposed `motor_timing.c`

   ```text
   convert microseconds to timer counts using measured timer clock; program bounded one-shot commutation event; interrupt ACK invokes original Rel17 event callback
   ```

5. `am13e_app_motor_sine_write` → proposed `motor_pwm.c`

   ```text
   use a/b/c phase indices and power to compute three-phase Rel17 sine duty; stage complementary outputs according to validated polarity, dead time, startup mode
   ```

6. `am13e_app_motor_sine_finish` → proposed `motor_pwm.c`

   ```text
   finish startup sine ownership and perform sequenced transition to six-step state at a known PWM boundary, preserving all hardware interlocks
   ```

7. `am13e_app_motor_sixstep_write` → proposed `motor_pwm.c`

   ```text
   decode U/V/W masks, floating phase, braking/damping and direction; produce one legal six-step commutation table entry; stage changes without shoot-through; verify no invalid positive/negative overlap
   ```

8. `am13e_app_motor_sixstep_idle` → proposed `motor_pwm.c`

   ```text
   set safe idle/freewheel behavior matching Rel17 neutral, preserving selected drag/active brake policy; do not force gate enable or silently replace active braking with coast
   ```

9. `am13e_app_motor_brushed_write` → proposed `motor_pwm.c`

   ```text
   translate Rel17 brushed forward/reverse and damp policy into a verified MCPWM pattern, with dead-time and complementary fault trip still active
   ```

10. `am13e_app_motor_commutation_commit` → proposed `motor_pwm.c`

   ```text
   atomically commit staged MCPWM phase/AQ/compare state at an appropriate hardware synchronization point; reject partially updated phase combinations
   ```

11. `am13e_app_motor_commutation_enable` → proposed `motor_power.c`

   ```text
   if enabling, require verified gate polarity, nFAULT inactive, no latched hardware trip, safe PWM state, validated comparator-to-PWM trip routing; enable exactly the required hardware path; if disabling, transition to known safe outputs
   ```

12. `am13e_app_motor_bemf_interval_select` → proposed `motor_bemf.c`

   ```text
   choose Rel17 BEMF timing/filter interval based on ertm_us; configure actual CMPSS0/1/3 COMPH and capture routing for the floating phase; blank switching transient; preserve polarity/edge semantics
   ```

13. `am13e_app_motor_bemf_stop` → proposed `motor_bemf.c`

   ```text
   mask/ACK comparator and timer events, disarm next BEMF commutation capture and clear pending timing state; do not disturb unrelated hardware fault trip
   ```

14. `am13e_app_motor_bemf_sine_exit_us` → proposed `motor_bemf.c`

   ```text
   schedule or cancel delay from sine startup to comparator-based commutation using real timer ticks and the original Rel17 0xffff sentinel semantics; qualify actual timer counter width
   ```

15. `compctl` → proposed `motor_bemf.c`

   ```text
   map Rel17 comparator-control argument to active BEMF phase/edge; synchronize CMPSS mux/filter, blanking and interrupt handling; distinguish BEMF zero-cross comparator from separate over-current hardware trip
   ```

16. `am13e_app_commutation_reset` → proposed `motor_pwm.c`

   ```text
   restore validated MCPWM output polarity/AQ state and safe commutation defaults after audio/beacon; clear software state consistently with original resetcom without disabling mandatory trip protection
   ```

17. `am13e_app_motor_fault_shutdown` → proposed `motor_fault.c`

   ```text
   on nFAULT, CMPSS over-current, watchdog or software fault: immediately force bridge inactive using hardware trip/verified PB13 path; latch fault and disallow restart; fault source ACK only when safe
   ```

18. `am13e_app_motor_fault_reset` → proposed `motor_fault.c`

   ```text
   after physical gate shutdown, request a deliberate MCU reset or enter permanent fault state with diagnosed cause; never return as if the shutdown succeeded when power outputs remain active
   ```

## Input watchdog (1)

Existing contract: `mcu/AM13E/io_backend.h`.

1. `am13e_app_io_watchdog_feed` → proposed `input_watchdog.c`

   ```text
   only call for CRC-valid DShot or validated PWM pulse from original src/io.c; refresh actual input-loss supervision deadline and reset policy; on missing fresh input, force Rel17 neutral/fault transition and power-stage-safe state; do not equate SysTick arming timer or independent IWDG with this command watchdog
   ```

## Historical UART telemetry (5) — not required for AM13E FW1

Existing contract: `mcu/AM13E/telem_backend.h`.

1. `am13e_telem_hw_init` → proposed `telem_uart.c`

   ```text
   configure the specified telemetry mode's real UART baud, inversion, half duplex, PA22/PA23 pin mux and RX DMA/IRQ; register the caller-owned RX buffer and ownership; use tested clock divisor
   ```

2. `am13e_telem_hw_pause_rx` → proposed `telem_uart.c`

   ```text
   pause UART RX/DMA without losing current buffer ownership; drain/ack pending interrupts and prepare physical half-duplex turnaround, as HoTT protocol requires
   ```

3. `am13e_telem_hw_tx_busy` → proposed `telem_uart.c`

   ```text
   report actual UART/DMA FIFO or shift-register TX-busy state, not merely whether application has queued bytes
   ```

4. `am13e_telem_hw_tx_byte` → proposed `telem_uart.c`

   ```text
   queue exactly one delayed HoTT byte, honor UART timing and last_byte completion/return-to-RX semantics; reject conflicting TX ownership
   ```

5. `am13e_telem_hw_tx_start` → proposed `telem_uart.c`

   ```text
   launch DMA/IRQ transmit of the original Rel17 buffer without early reuse; ACK actual completion and invoke am13e_telem_on_tx_done() once; restore RX if required
   ```

## Audio / motor-powered tones (7)

Existing contract: `mcu/AM13E/util_backend.h`.

1. `am13e_app_audio_music_begin` → proposed `audio_motor.c`

   ```text
   acquire MCPWM audio mode only when Rel17 is stopped; preserve motor fault trip, valid dead-time and output polarity; set note timing hardware ownership
   ```

2. `am13e_app_audio_music_note` → proposed `audio_motor.c`

   ```text
   translate Rel17 semitone and octave shift with volume into actual timer period and bridge modulation; use validated PWM limits and synchronization
   ```

3. `am13e_app_audio_music_pause` → proposed `audio_motor.c`

   ```text
   produce electrically safe silence while retaining ownership and physical trip protection; preserve original score pause length
   ```

4. `am13e_app_audio_music_tick` → proposed `audio_motor.c`

   ```text
   service Rel17 note/bridge commutation at the intended cadence during delayf; do not simulate ticks or feed the input watchdog
   ```

5. `am13e_app_audio_pcm_begin` → proposed `audio_motor.c`

   ```text
   configure sample clock and safe motor bridge audio PWM for real AU PCM rate_hz and volume; serialize with motor mode
   ```

6. `am13e_app_audio_pcm_sample` → proposed `audio_motor.c`

   ```text
   apply signed 8-bit PCM sample using real sample-ready event; never silently drop or advance without pacing; ensure ISR/DMA ownership and fault handling
   ```

7. `am13e_app_audio_end` → proposed `audio_motor.c`

   ```text
   stop timed sound/PCM outputs and return MCPWM ownership to the validated idle motor state; preserve fault latch and commutation reset sequencing
   ```

## FW1 configuration flash (1)

Existing contract: `mcu/AM13E/util_backend.h`.

1. `am13e_app_cfg_commit` → proposed `config_flash.c`

   ```text
   validate destination equals FW1 config partition 0x4000..0x4fff and source/byte count, reject FW2 0x5000..0x5fff and Boot 0x0000..0x3fff; align erase to physical 2KiB sectors; run TI flash programming from RAM with interrupts/flash ECC constraints; verify readback and fail on power-loss or programming error; return nonzero ONLY on verified success
   ```

## TI SDK 26.01.00.03 — Implementation Porting Guide

**Read this before implementing any of the 32 callbacks.** The existing symbol list above is a behavioral *TODO*. The entries below add original Rel17 call sites, SDK-declared API names, suggested porting actions, and actual test gates. Each entry is a C-comment-form **instruction**, not implementation or a claim that hardware works.

**Evidence labels**: [REL17] = call site grounded in current FW1 linker/source; [SDK DECLARED] = name confirmed in the uploaded `am13e2x_sdk-main.zip` under `source/driverlib/am13e230x/`; [PORTING PROPOSED] = engineering plan, not implemented; [HW VALIDATION] = a required silicon/board demonstration.

**Build state (2026-10-10):** E1-AK ARM GNU Object Build PASS, zero warning; FW1 strict link remains **27 unique symbols / 38 references**. E1-AL updates parity/field-range checks; Host/ARM regression results pending. Five actual UART Backend symbols remain unresolved.

### Common porting method and TI SDK mapping

1. Read the Rel17 call site and check *units, owner, trigger and return semantics* in `src/main.c`, `src/io.c`, `src/telem.c`, `src/util.c`. For PB14-only FW1, preserve the `sendtelem()` DShot data producer while excluding UART protocol control only; preserve behavior on all other Rel17 targets.
2. Check exact C signature in `mcu/AM13E/{motor,io,telem,util}_backend.h`; `compctl(int)` is declared in `src/common.h`. Do not invent new register-facing prototypes in application code.
3. Check DriverLib's *header and signature*, relevant parameter struct, clock, IRQ flag/ACK and reset/Power Domain requirement. Verified SDK headers include `dl_mcpwm.h`, `dl_cmpss_lite.h`, `dl_xbar.h`, `dl_timer.h`, `dl_dma.h`, `dl_unicommuart.h`, `dl_unicomm.h`, `dl_flashctl.h`, `dl_flash.h`, `dl_wwdt.h`, `dl_gpio.h`, `dl_sysctl.h`. Names being present does **not** prove their configuration or physical route is correct.
4. Construct a backend owner for each MCU resource: MCPWM0 + Trip Zone; comparator/filter and BEMF; commutation timer and IRQ; separate bidirectional command input; UART DMA + half-duplex; audio mode; Flash/ECC. Document ISR ownership, preemption and failure behavior.
5. Confirm HW Baseline pins: MCPWM0 U PA8/PA11, V PA9/PA30, W PA10/PA31; PB13 GPIO45 power gate (polarity unverified); PB15 GPIO47 nFAULT; PB14 GPIO46 DShot RX/BiDShot TX (not 5-V tolerant). Physical hardware trip is **CMPSS→PWMXBAR→MCPWM**, *not* PB15 software IRQ alone. Do not conflate BEMF comparators with the overcurrent trip comparator.
6. **Known SDK mismatches:** Device TRM maps DMA index 39 to ECAP0 but SDK labels it `DL_DMA_TRIGGER_SOURCE_ECAP1DMA`; `DL_DMA_INTERRUPT_DATA_ERROR` expands to absent `DMA_IMASK_DATAERR_SET` in this AM13E230x device header. Validate device registers against TRM, never copy generic enum names blindly.
7. Rel17 command watchdog, MCU WWDT and 250ms neutral arming timer are distinct features. Do not satisfy an unimplemented input watchdog by refreshing WWDT unconditionally.
8. Firmware config range is FW1 `0x4000..0x4fff`, FW2 `0x5000..0x5fff`. `dl_flashctl.h` declares `DL_FLASHCTL_SECTOR_SIZE (2048U)`; verify erase/program/ECC and RAM-execution rules and never touch frozen Boot or FW2 by accident.

### Per-symbol commented instructions (32 symbols)

#### `am13e_app_motor_init`

```c
// [REL17] src/main.c:713
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_initParamsSetDefault, DL_MCPWM_init, DL_MCPWM_configureTimeBase, DL_MCPWM_configureDeadBand, DL_MCPWM_configureTripZone, DL_XBAR_selectPWMXBARSource
// [PORTING PROPOSED] Configure real inactive MCPWM0 outputs and trip wiring before six PWM pins/gate power can be enabled; initialize validated BEMF comparator paths separately
// [HW VALIDATION] Scope six outputs, dead time and fault trip with gate drive isolated; verify PB13 polarity
```

#### `am13e_app_motor_runtime_enable_interrupts`

```c
// [REL17] src/main.c:750
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_clearInterrupt, DL_MCPWM_enableInterrupt, DL_SYSCTL_getClockStatus, NVIC_SetPriority, NVIC_EnableIRQ
// [PORTING PROPOSED] Check VTOR/IRQ ownership, MCLK and MCU fault routes, hardware trip, inactive PWM and PB15 nFAULT; only then unmask PRIMASK
// [HW VALIDATION] Bad clock/vector/nFAULT/Trip Zone must prevent enable and reach fail-closed path
```

#### `am13e_app_motor_pwm_apply`

```c
// [REL17] src/main.c:934
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_setTimeBasePeriodShadow, DL_MCPWM_setCounterCompareShadowValue, DL_MCPWM_setCounterCompareShadowLoadMode
// [PORTING PROPOSED] Convert Rel17 logical duty 0..2000 plus variable freq, ertm, damping, lock and brushed into synchronized PWM shadow register updates
// [HW VALIDATION] Sweep duty and 16..96kHz configured range; compare complementary timing to Rel17
```

#### `am13e_app_motor_sine_schedule_us`

```c
// [REL17] src/main.c:141
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_Timer_initTimerMode, DL_Timer_enableInterrupt, DL_Timer_startCounter, DL_Timer_clearInterruptStatus
// [PORTING PROPOSED] Convert logical microseconds to measured timer ticks; program one-shot and IRQ ACK then invoke Rel17 commutation callback exactly once
// [HW VALIDATION] Capture scheduled delays/jitter at sine startup range
```

#### `am13e_app_motor_sine_write`

```c
// [REL17] src/main.c:157
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_setCounterCompareShadowValue, DL_MCPWM_configureActionQualifierActions
// [PORTING PROPOSED] Calculate phase compare values from original Rel17 sinedata[] and a/b/c indices/power; preserve safe complementary actions
// [HW VALIDATION] Check all phase indices at low/high power and no output overlap
```

#### `am13e_app_motor_sine_finish`

```c
// [REL17] src/main.c:172
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_configureLoadMode, DL_MCPWM_forceGlobalLoadOneShotEvent
// [PORTING PROPOSED] Handover sine startup to six-step on a PWM synchronization event; keep trip latched
// [HW VALIDATION] Scope sine-to-commutation transition for spurious output pulses
```

#### `am13e_app_motor_sixstep_write`

```c
// [REL17] src/main.c:215
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_configureActionQualifierActions, DL_MCPWM_setCounterCompareShadowValue
// [PORTING PROPOSED] Build six-step truth table for positive/negative/floating phase and damp/reverse; stage legal waveform without activating partial phase state
// [HW VALIDATION] Exhaust 6 steps and reverse, brake, coast and illegal-mask cases
```

#### `am13e_app_motor_sixstep_idle`

```c
// [REL17] src/main.c:384
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_setActionQualifierSWAction, DL_MCPWM_configureActionQualifierActions
// [PORTING PROPOSED] Produce Rel17 neutral mode including configured active brake vs coast; preserve external trip
// [HW VALIDATION] Scope all six outputs for neutral, drag, active brake
```

#### `am13e_app_motor_brushed_write`

```c
// [REL17] src/main.c:953
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_configureActionQualifierActions, DL_MCPWM_setCounterCompareShadowValue
// [PORTING PROPOSED] Translate original brushed reverse and damp into MCPWM outputs with verified dead time/physical polarity
// [HW VALIDATION] Capture forward, reverse and braking transitions without shoot-through
```

#### `am13e_app_motor_commutation_commit`

```c
// [REL17] src/main.c:396,998
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_enableGlobalLoad, DL_MCPWM_setGlobalLoadTrigger, DL_MCPWM_forceGlobalLoadOneShotEvent
// [PORTING PROPOSED] Atomically latch prepared phase, action qualifier and compare values on a known PWM boundary
// [HW VALIDATION] Measure phase sequencing with IRQ contention and fault during update
```

#### `am13e_app_motor_commutation_enable`

```c
// [REL17] src/main.c:999,1017
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_getTripZoneFlagStatus, DL_MCPWM_enableTripZoneSignals, DL_GPIO_readPins, DL_GPIO_setPins
// [PORTING PROPOSED] Enable only after Rel17 arm criteria, inactive PB15 nFAULT, verified PB13 polarity and independent hardware trip; disable to physically safe state
// [HW VALIDATION] Inject trip and gate fault before and during enable; reject bad hardware state
```

#### `am13e_app_motor_bemf_interval_select`

```c
// [REL17] src/main.c:318
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_CMPSSLITE_configHighComparator, DL_CMPSSLITE_configFilterHigh, DL_CMPSSLITE_enableModule, DL_XBAR_selectOutputXBARSource
// [PORTING PROPOSED] Select the actual floating phase CMPSS0/1/3 and zero-cross edge, blank switching spikes and arm capture timing; BEMF is not overcurrent trip
// [HW VALIDATION] Inject phase U/V/W BEMF and confirm polarity, filtering, interval timestamps
```

#### `am13e_app_motor_bemf_stop`

```c
// [REL17] src/main.c:1018
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_CMPSSLITE_getStatus, DL_Timer_disableInterrupt, DL_Timer_clearInterruptStatus
// [PORTING PROPOSED] Cancel BEMF/commutation events and acknowledge IRQ without disabling separate hardware overcurrent protection
// [HW VALIDATION] No stale zero-cross events; hardware overcurrent trip remains effective
```

#### `am13e_app_motor_bemf_sine_exit_us`

```c
// [REL17] src/main.c:894,1000
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_Timer_initTimerMode, DL_Timer_stopCounter, DL_Timer_startCounter
// [PORTING PROPOSED] Implement sine-exit timeout/cancel from actual timer rate; examine Rel17 meaning of 0xffff sentinel before deciding handling
// [HW VALIDATION] Test 0, typical, 0xffff and rapid repeated scheduling
```

#### `compctl`

```c
// [REL17] src/main.c:188,302,400; src/common.h
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_CMPSSLITE_configHighComparator, DL_CMPSSLITE_configFilterHigh, DL_SYSCTL_setCompartorHPMux, DL_XBAR_selectOutputXBARSource
// [PORTING PROPOSED] First reverse-map original compctl phase-mask semantics; select the real comparator mux/edge/filter while preserving independent fault path
// [HW VALIDATION] Verify all 6 BEMF commutation edges plus forced-zero/stop inputs
```

#### `am13e_app_commutation_reset`

```c
// [REL17] src/util.c:606
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_setActionQualifierSWAction, DL_MCPWM_configureActionQualifierActions
// [PORTING PROPOSED] Restore resetcom output and software state after audio while retaining trip and idle behavior
// [HW VALIDATION] No drive pulse during resetcom, including faulted and audio cases
```

#### `am13e_app_motor_fault_shutdown`

```c
// [REL17] src/main.c:595
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_MCPWM_getTripZoneFlagStatus, DL_MCPWM_setTripZoneAction, DL_MCPWM_enableTripZoneSignals, DL_GPIO_clearPins
// [PORTING PROPOSED] Make comparator-to-PWMXBAR-to-MCPWM fault independent of CPU; immediately force proven inactive bridge and latch fault; PB13 action only after polarity verification
// [HW VALIDATION] Scope shutdown with CPU IRQ masked, test overcurrent plus PB15 secondary path
```

#### `am13e_app_motor_fault_reset`

```c
// [REL17] src/main.c:600
// [SDK HEADERS] dl_mcpwm.h / dl_cmpss_lite.h / dl_xbar.h / dl_timer.h / dl_gpio.h / dl_sysctl.h
// [SDK DECLARED] DL_SYSCTL_resetDevice, DL_MCPWM_getTripZoneFlagStatus
// [PORTING PROPOSED] After verified bridge shutdown, diagnose/reset or trap permanently; do not return into motor code as success
// [HW VALIDATION] Reset during trip and retained/clearable fault states
```

#### `am13e_app_io_watchdog_feed`

```c
// [REL17] src/io.c:534,542
// [SDK HEADERS] dl_wwdt.h / dl_timer.h
// [SDK DECLARED] DL_WWDT_initWatchdogMode, DL_WWDT_restart, DL_Timer_getTimerCount
// [PORTING PROPOSED] Supervise only Rel17 validated DShot CRC/PWM command events; distinguish command-loss supervision from hardware WWDT and 250ms arming timer
// [HW VALIDATION] CRC corrupt frames must not refresh timeout; motor enters safe response on signal loss
```

#### `am13e_telem_hw_init`

```c
// [REL17] src/telem.c:60
// [SDK HEADERS] dl_unicommuart.h / dl_unicomm.h / dl_dma.h
// [SDK DECLARED] DL_UNICOMM_setIPMode, DL_UART_init, DL_UART_setClockConfig, DL_UART_configBaudRate, DL_UART_setDirection
// [PORTING PROPOSED] Configure PA22/PA23 UART and protocol specific baud, inversion/duplex, RX buffer, IRQ and DMA ownership; preserve Rel17 telem_mode selection
// [HW VALIDATION] KISS/iBUS/S.Port/CRSF/MSB/HoTT baud and frame timing
```

#### `am13e_telem_hw_pause_rx`

```c
// [REL17] src/telem.c:338
// [SDK HEADERS] dl_unicommuart.h / dl_unicomm.h / dl_dma.h
// [SDK DECLARED] DL_UART_disableDMAReceiveEvent, DL_UART_disableInterrupt, DL_UART_isRXFIFOEmpty, DL_DMA_disableChannel
// [PORTING PROPOSED] Pause RX only after account for DMA in-flight bytes, IRQ acknowledgement and half-duplex turn; preserve caller buffer
// [HW VALIDATION] HoTT RX->TX turnaround with burst traffic and no lost/misowned bytes
```

#### `am13e_telem_hw_tx_busy`

```c
// [REL17] src/telem.c:347
// [SDK HEADERS] dl_unicommuart.h / dl_unicomm.h / dl_dma.h
// [SDK DECLARED] DL_UART_isBusy, DL_UART_isTXFIFOEmpty
// [PORTING PROPOSED] Use true peripheral TX and DMA activity, not software queue state alone
// [HW VALIDATION] No TX buffer reuse before final UART stop bit
```

#### `am13e_telem_hw_tx_byte`

```c
// [REL17] src/telem.c:438
// [SDK HEADERS] dl_unicommuart.h / dl_unicomm.h / dl_dma.h
// [SDK DECLARED] DL_UART_transmitData, DL_UART_getRawInterruptStatus, DL_UART_isBusy
// [PORTING PROPOSED] Transmit exactly one deferred HoTT byte on real UART, retain pacing and last_byte semantics
// [HW VALIDATION] Logic analyzer measures inter-byte delay and RX recovery
```

#### `am13e_telem_hw_tx_start`

```c
// [REL17] src/telem.c:363,487
// [SDK HEADERS] dl_unicommuart.h / dl_unicomm.h / dl_dma.h
// [SDK DECLARED] DL_UART_enableDMATransmitEvent, DL_DMA_setSrcAddr, DL_DMA_setTransferSize, DL_DMA_enableChannel
// [PORTING PROPOSED] Asynchronously send Rel17 buffer, hold ownership until physical DMA/UART complete and invoke am13e_telem_on_tx_done once
// [HW VALIDATION] Stress multi-frame transmissions and half-duplex conflicts
```

#### `am13e_app_audio_music_begin`

```c
// [REL17] src/util.c:660
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_MCPWM_configureTimeBase, DL_MCPWM_configureActionQualifierActions, DL_Timer_initTimerMode
// [PORTING PROPOSED] Acquire MCPWM for motor-powered music only when motor idle, retain dead time and hardware trip
// [HW VALIDATION] No unexpected gate drive when score starts
```

#### `am13e_app_audio_music_note`

```c
// [REL17] src/util.c:698
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_MCPWM_setTimeBasePeriodShadow, DL_MCPWM_setCounterCompareShadowValue
// [PORTING PROPOSED] Convert Rel17 semitone, octave and volume to real PWM period/compare without raw STM32 TIM registers
// [HW VALIDATION] Measure notes, octave, duty and amplitude versus Rel17
```

#### `am13e_app_audio_music_pause`

```c
// [REL17] src/util.c:682
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_MCPWM_setActionQualifierSWAction, DL_MCPWM_setCounterCompareShadowValue
// [PORTING PROPOSED] Implement score pause as inactive physical waveform with original duration; preserve trip
// [HW VALIDATION] Verify silence and no accidental gate pulse
```

#### `am13e_app_audio_music_tick`

```c
// [REL17] src/util.c:632
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_Timer_getRawInterruptStatus, DL_Timer_clearInterruptStatus, DL_MCPWM_setCounterCompareShadowValue
// [PORTING PROPOSED] Service real PWM/sample cadence during delayf, independent of input watchdog and arming refresh
// [HW VALIDATION] One commutation event per intended tick, no synthetic time
```

#### `am13e_app_audio_pcm_begin`

```c
// [REL17] src/util.c:733
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_Timer_initTimerMode, DL_Timer_enableInterrupt, DL_MCPWM_configureTimeBase
// [PORTING PROPOSED] Program AU PCM sample clock and valid motor output ownership, preserve hardware trip
// [HW VALIDATION] Check configured sample rate and waveform under low volume
```

#### `am13e_app_audio_pcm_sample`

```c
// [REL17] src/util.c:761
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_Timer_getRawInterruptStatus, DL_Timer_clearInterruptStatus, DL_MCPWM_setCounterCompareShadowValue
// [PORTING PROPOSED] Wait for or queue actual sample period; map signed 8-bit PCM to safe PWM without skipping
// [HW VALIDATION] Compare recorded PWM against known PCM fixture and verify no underrun
```

#### `am13e_app_audio_end`

```c
// [REL17] src/util.c:717,774
// [SDK HEADERS] dl_mcpwm.h / dl_timer.h
// [SDK DECLARED] DL_Timer_stopCounter, DL_MCPWM_setActionQualifierSWAction, DL_MCPWM_configureLoadMode
// [PORTING PROPOSED] Stop audio timer, return bridge ownership to neutral motor state without clearing fault
// [HW VALIDATION] End sound during idle and forced fault; no drive glitch
```

#### `am13e_app_cfg_commit`

```c
// [REL17] src/util.c:550
// [SDK HEADERS] dl_flashctl.h / dl_flash.h
// [SDK DECLARED] DL_FLASHCTL_SECTOR_SIZE, DL_FlashCTL_eraseMemory, DL_FlashCTL_programMemory128WithECCGenerated, DL_FlashCTL_readVerify128WithECCGenerated, DL_FlashCTL_getCommandStatus
// [PORTING PROPOSED] Validate [destination, length] wholly within FW1 config 0x4000..0x4fff; 2048-byte sector boundaries; RAMFUNC/ECC-safe erase and program; readback before success; preserve FW2 and Boot
// [HW VALIDATION] Power-cycle persistence, boundary writes, power-fail injection, FW2 0x5000 and Boot unchanged
```

### WSL development and evidence workflow

```bash
SDK="${AM13E_SDK_ROOT:-$HOME/ti/am13e230x_sdk_26_01_00_03}"
rg -n 'DL_MCPWM_configureTripZone|DL_XBAR_selectPWMXBARSource' \
  "$SDK/source/driverlib/am13e230x"/{dl_mcpwm.h,dl_xbar.h}
rg -n 'DL_CMPSSLITE_configHighComparator|DL_UART_init|DL_FlashCTL_eraseMemory' \
  "$SDK/source/driverlib/am13e230x"/{dl_cmpss_lite.h,dl_unicommuart.h,dl_flashctl.h}
cmake --build build-am13e --target AM13E -j"$(nproc)"
cmake --build build-am13e --target AM13E_FW1.elf -j"$(nproc)"
```

Review order for each real implementation: Rel17 equivalent behavior → verified SDK signature/register mapping → own ISR/DMA resource and failure path → ARM GNU compile → strict FW1 link → oscilloscope/electrical acceptance. **Never add empty C bodies or fake success returns to make a missing symbol disappear.**

### E1-AM UC2 UART hardware staging (source integration only)

- `telem_uc2_preflight.{c,h}` adds real DriverLib UC2 power-on, UART clock at nominal BUSCLK 100MHz, baud configuration and 8N1, disabled UART TX/RX direction, and readback. Both PA22/PA23 remain GPIO INPUT / MCU Hi-Z. It **must not** be called as full telemetry initialization, nor treated as resolved `am13e_telem_hw_init`.
- SDK PinMux: `PA22=IOMUX_PA22_UC2_TX_SDA (4)`, `PA23=IOMUX_PA23_UC2_RX_SCL (4)`. UC2 is a two-pin UART peripheral; the product Rel17 single-wire line needs a qualified external coupling/bidirectional circuit, potentially pad inversion and drive ownership. **No external schematic-qualified PHY has yet been demonstrated.**
- The SDK does expose IOMUX pin inversion (`DL_GPIO_setDataInversion()`, `DL_GPIO_initPeripheral...Features()`) but pin-level inverted serial correctness is unverified; there is no direct `HDSEL` UART setting equivalent proven on UC2. RS485 mode must not be assumed equivalent.
- `DL_UART_INTERRUPT_EOT_DONE` exists; future byte/DMA scheduler must defer `am13e_telem_on_tx_done()` until the actual last stop bit, not only the FIFO/DMA write. For received packets, hardware `RXTOSEL` is a 0..15 field and Rel17 S.Port `RTOR=26` requires independent qualified gap timing.
- **E1-AM ARM GNU result: pending**. This module does not implement any of the five Telemetry callback symbols, does not request a live UC2 IRQ, and changes no physical TX pin state.

## Implementation order and acceptance gates

1. **Compiler regression fixed in E1-Z.** The E1-Y SDK macro `DL_DMA_INTERRUPT_DATA_ERROR` expands to `DMA_IMASK_DATAERR_SET`, absent from AM13E230x `hw_dma.h`. The corrected Channel Completion / Address Error masks compile cleanly; retain this mismatch as a SDK caveat.
2. **Motor fault foundation**: establish real `am13e_app_motor_fault_shutdown`, `fault_reset`, power/GPIO/MCPWM inactive and hardware Trip Zone, then `motor_init` and `runtime_enable_interrupts`. Validate inactive outputs and fault state with supply/gate-drive appropriately isolated.
3. **Motor PWM/commutation**: implement full Rel17 sine, six-step, brush/damp, shadow-update and logical duty/frequency behavior; verify complementary interlock and trip response with scope.
4. **BEMF**: implement comparator selection, edge/filter/blanking and timed callbacks; verify capture timestamps, zero-cross polarity and commutation event sequence.
5. **Input watchdog, UART telemetry, Flash settings and Audio**: implement actual peripherals and verify no functional regression to Rel17. Flash test must prove FW2 config and Boot remain untouched.
6. **FW1 strict linker**: compile with project warnings; link with `-Wl,--no-undefined` and correct vector names. Record final ELF/map and memory/Flash ranges. **Link PASS is not hardware-functional PASS.**
7. **HW acceptance**: DShot150/300/600 and bidirectional reply waveform; 30us turnaround with physical level converter; BEMF; MCPWM trip/inactive polarity; nFAULT; current/voltage/thermal; watchdog loss of signal; telemetry protocols; audio; config power-cycle persistence.

### Useful review checklist for each symbol

```text
[ ] Exact existing function prototype / Rel17 call site and units confirmed
[ ] Product HW mapping vs TI SDK routing verified
[ ] Interrupt/event ACK and concurrency policy implemented
[ ] Missing hardware requirement/threshold explicitly documented
[ ] Safe failure behavior preserves motor shutdown and protects FLASH
[ ] ARM GNU object compile (no project warnings)
[ ] Strict FW1 link changes are explained with symbol diff
[ ] Hardware test waveform / log / acceptance data attached
```

**Important:** All illustrative pseudocode above remains descriptive; the separate new MCU C files listed in the current ledger contain actual source implementations awaiting ARM GNU/hardware verification. This Markdown file alone does not influence linking.
