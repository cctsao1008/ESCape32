# AM13E Rel17 FW1 — Missing Backend Implementation Pseudocode

**Status:** Design / TODO; intentionally **NOT compiled**; no MCU callback stubs or fake return values.
**Source of missing-symbol evidence:** `e1x-fw1-link.log` (32 distinct unresolved symbols, 44 linker references). The later E1-Y logs do **not** constitute a completed strict-link run because the DMA macro error stopped object compilation. Do not claim the symbol set is an exhaustive final ELF audit until compilation and link are rerun.
**Reference boundaries:** Original ESCape32 Rel17 `src/main.c`, `src/io.c`, `src/telem.c`, `src/util.c`, `src/prog.c`; AM13E target-specific contracts in `mcu/AM13E/*.h`; AM13E230x TI TRM/SDK; project SW/HW architecture baseline v1.6.

## Contract and integration rules

- A **function mentioned here does not have an implementation**. Never add `return 1;`, empty `void` bodies, or dummy callback handlers to satisfy `-Wl,--no-undefined`.
- Keep original Rel17 application as the only policy owner, and all AM13E register/clock/interrupt/pin handling in `mcu/AM13E/`. This document is a *plan*, not an alternate firmware.
- Keep exact existing prototypes in `motor_backend.h`, `io_backend.h`, `telem_backend.h` and `util_backend.h`; `compctl(int)` is declared in `src/common.h`.
- **Motor bridge safety:** MCPWM0 outputs PA8/PA11, PA9/PA30, PA10/PA31; PB13 GPIO45 gate enable (electrical polarity NOT yet qualified); PB15 GPIO47 nFAULT active-low backup input; HW comparator → PWMXBAR → MCPWM trip is mandatory and not replaced by PB15 software IRQ or SysTick polling.
- BEMF uses CMPSS0 COMPH PA17/PA4, CMPSS1 COMPH PA3/PA2, CMPSS3 COMPH PA16/PA18 per HW v1.6. Do not assume these BEMF comparators are automatically the *over-current* trip source; select/qualify the actual protection comparator, threshold, XBAR source and shutdown polarity separately.
- The 25MHz external crystal / nominal 200MHz MCLK is already handled by the existing clock backend. Use the actual peripheral clock and verified timer counts, not nominal CPU cycles guessed for individual modules.
- FW1 configuration region is 0x00004000–0x00004fff, FW2 settings are 0x00005000–0x00005fff, and the application is 0x00006000–0x0007ffff. Boot is frozen; preserve image/flash contracts.
- Do not remove DShot RX, BiDShot TX, music/audio, config persistence or telemetry from functional scope. Some items are source-integrated and others remain unverified; implementation and hardware qualification are distinct milestones.
- **No SDK board example wiring is authoritative for the product.** Confirm selected MCU peripheral mux, interrupts, DMA channels, output polarities and system-level protection on the board.

## Missing-symbol register (32 symbols / 44 references)

| Area | Count | Dependency |
|---|---:|---|
| Motor / MCPWM / BEMF / Protection | 18 | MCPWM0, CMPSS, PWMXBAR, interrupts, gate driver and Trip Zone |
| Command input watchdog | 1 | Valid-frame supervision / timeout reaction |
| Telemetry / UART | 5 | Product UART pinmux, DMA, half duplex, protocol timing |
| Audio / motor-powered tones | 7 | Rel17 music/PCM timing, MCPWM ownership |
| FW1 configuration flash | 1 | Flash sector/ECC, RAMFUNC, partition verification |
| **Total** | **32** | Existing `--no-undefined` gate retained |

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

## Telemetry / UART (5)

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

## Implementation order and acceptance gates

1. **Fix toolchain regression first.** The E1-Y SDK macro `DL_DMA_INTERRUPT_DATA_ERROR` expands to `DMA_IMASK_DATAERR_SET`, absent from AM13E230x `hw_dma.h`. Preserve implemented Channel Completion / Address Error masks; verify the corrected object compile before treating a new link log as current.
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

**Important:** All pseudocode above is intentionally *descriptive*, not buildable C. This file does not change the number of unresolved symbols; each symbol is resolved only by its later real MCU backend.
