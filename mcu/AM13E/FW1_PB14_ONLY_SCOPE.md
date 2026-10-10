# AM13E FW1 External I/O Scope — PB14 only

**Product constraint (confirmed):** ESCape32 application FW1 uses **PB14 only** for external command and telemetry I/O. It does **not** use the UNICOMM2/PA22/PA23 UART telemetry transport. This file describes the product target, not other ESCape32 Rel17 board variants.

| Path | FW1 | Implementation |
|---|---|---|
| PB14 GPIO46 — PWM/DShot command RX | Included | ECAP0 RX, DShot CRC/command handling |
| PB14 GPIO46 — Bidirectional DShot telemetry TX | Included | Rel17 GCR/NRZI codec, TIMG4/DMA0 toggles; final-edge-anchored 30us turnaround/rejection is shared with host-tested pure timing logic (physical ISR/DMA latency, waveform and contention still to validate) |
| Extended DShot telemetry scheduler | Included | **Original `src/telem.c::sendtelem()`** remains; cycles ESC temperature, motor temperature, voltage, current into `dshotval` every 512 SysTicks when `dshotext` is enabled |
| PB14 watchdog supervision | Included | WWDT0 fed only after accepted receiver frames; HW behavior still to validate |
| USART/UNICOMM2 telemetry (KISS, iBUS, S.Port, CRSF, MSB, HoTT) | **Not used by FW1** | Original Rel17 implementations retained for other boards; excluded from AM13E FW1 build |
| Serial Telemetry TX IO reservation | **IO-only** | `AM13E_BOARD_SERIAL_TX_PINCM=0` unassigned; if assigned, configure input/Hi-Z only, without USART/UNICOMM, TX DMA or telemetry stack |

## Compilation and ownership

- Define `AM13E_PB14_ONLY` only on the TI AM13E *application* target, not on Boot or legacy MCU targets.
- Keep `src/telem.c` in the object build, but conditionally exclude the legacy serial/UART protocol handlers, `inittelem()`, and `sendtelemdata()`. **Do not delete `sendtelem()`: its Extended DShot part produces PB14 bidirectional telemetry data.**
- `src/main.c` skips `inittelem()` for the PB14-only target; `pend_sv_handler()` continues calling `sendtelem()` on SysTick, preserving the original 32ms Extended DShot scheduler (16kHz / 512).
- `src/io.c` already excludes STM32 UART serial parser under `!AM13E`. Its shared DShot command decoder, `dshotext` flag, `dshotval` and PB14 GCR/NRZI path remain untouched.
- Do not compile `telem_mode_plan.c`, `telem_uc2_preflight.c`, or SDK `dl_unicomm.c` and `dl_unicommuart.c` into the FW1 target. Those earlier investigative sources remain in the repository for history; they are **not** FW1 deliverables.
- The five unresolved `am13e_telem_hw_*` UART symbols are **out of scope**, not to be implemented or replaced with fake no-op functions for this FW1 target.
- The remaining Motor/Protection, Audio and Config Flash symbols must still use real backends; `--no-undefined` remains enabled.
- E1-AN ARM GNU object compile **PASS** (self-owned code 0 warning; TI SDK `dl_timer.c` 10 `-Wmaybe-uninitialized` warnings). E1-AN Strict Link: **22 distinct Undefined Symbols / 32 references**, down from E1-AL 27/38. The five UART callbacks are no longer linked and PB14 Extended DShot code remains.
- E1-AO adds a standalone FW1 settings Flash write-range/ECC planner with host evidence; it does not implement Flash erase or programming, and does not resolve `am13e_app_cfg_commit`. Rebuild before updating FW1 compile evidence.

## Mandatory scope checks

```bash
# Source-level: visible PB14 schedule and absence of UC2 dependencies.
rg -n 'AM13E_PB14_ONLY|Extended DSHOT telemetry|am13e_app_io_bidir_telemetry_levels' \
  CMakeLists.txt src/main.c src/telem.c src/io.c
rg -n 'telem_uc2_preflight.c|dl_unicommuart.c|dl_unicomm.c' CMakeLists.txt

cmake --build build-am13e --target AM13E -j"$(nproc)"
cmake --build build-am13e --target AM13E_FW1.elf -j"$(nproc)"
```

**Never treat a missing UART undefined reference as a substitute for testing PB14**. DShot150/300/600 CRC RX, Extended DShot replies, BiDShot 30µs turnaround, 3.3V/5V contention-safe interface and RX recovery remain physical acceptance gates.

## ECAP0 acknowledgement-epoch receive integrity

The FW1 ECAP0 IRQ requires all CEVT1..4 flags, then ACKs the old
capture epoch **before** copying CAP1..4; a second event-flag snapshot
and modulo slot check rejects any racing capture while copying.
Discarded groups reset partial DShot state and are not forwarded
to Rel17 or WWDT. The previous pure-capture timing check and
DShot150/300/600 + BiDShot end-to-end host tests remain enabled.

**Limit:** Hardware overwrites of entire four-edge groups before the
ISR first reads ECFLG cannot be ruled out by firmware flag checks.
DShot600 input frequency may exceed the realistic MCU ISR throughput
when paired with commutation/BEMF activity; a measured ISR worst-case
execution-time budget or a verified DMA capture architecture remains
an **on-target** acceptance requirement. This is reported separately
from the now-passing software coverage audit.

### DShot capture freshness and measurable diagnostics

The actual ECAP0 IRQ now reads TSCTR after snapshotting CAP1–CAP4
and **rejects DShot pairs older than two calibrated DShot bit periods**.
This is the next four-event CEVT4 arrival budget. It is a conservative
software admission check, not proof of deterministic ISR capacity.

`AM13E_PB14_Status` now exposes `late_capture_groups` and
`max_capture_age_ticks`, alongside `capture_overruns`, for on-target
measurements. A rejected pair drops the partial frame instead of
feeding Rel17 or WWDT. The native tests exercise DShot150/300/600,
deadline boundaries, uint32 counter wrap and servo exemption.

At DShot600, a two-bit group arrives in approximately 3.33us; at
200MHz that is only about 667 MCU cycles **before other interrupt
costs**. The present RX backend uses ECAP0 CEVT4 IRQ rather than
DMA capture. It is therefore not legitimate to declare physical
high-rate RX loss-free without a measured ISR budget or a tested DMA
buffered capture path.
