# AM13E FW1 External I/O Scope — PB14 only

**Product constraint (confirmed):** ESCape32 application FW1 uses **PB14 only** for external command and telemetry I/O. It does **not** use the UNICOMM2/PA22/PA23 UART telemetry transport. This file describes the product target, not other ESCape32 Rel17 board variants.

| Path | FW1 | Implementation |
|---|---|---|
| PB14 GPIO46 — PWM/DShot command RX | Included | ECAP0 RX, DShot CRC/command handling |
| PB14 GPIO46 — Bidirectional DShot telemetry TX | Included | Rel17 GCR/NRZI codec, TIMG4/DMA0 toggles, direction and RX restoration (HW timing still to validate) |
| Extended DShot telemetry scheduler | Included | **Original `src/telem.c::sendtelem()`** remains; cycles ESC temperature, motor temperature, voltage, current into `dshotval` every 512 SysTicks when `dshotext` is enabled |
| PB14 watchdog supervision | Included | WWDT0 fed only after accepted receiver frames; HW behavior still to validate |
| USART/UNICOMM2 telemetry (KISS, iBUS, S.Port, CRSF, MSB, HoTT) | **Not used by FW1** | Original Rel17 implementations retained for other boards; excluded from AM13E FW1 build |
| PA22/PA23 UART pin mux | **Not owned by FW1 telemetry** | No UC2 pin mux/enable or UART driver is linked or called by FW1 |

## Compilation and ownership

- Define `AM13E_PB14_ONLY` only on the TI AM13E *application* target, not on Boot or legacy MCU targets.
- Keep `src/telem.c` in the object build, but conditionally exclude the legacy serial/UART protocol handlers, `inittelem()`, and `sendtelemdata()`. **Do not delete `sendtelem()`: its Extended DShot part produces PB14 bidirectional telemetry data.**
- `src/main.c` skips `inittelem()` for the PB14-only target; `pend_sv_handler()` continues calling `sendtelem()` on SysTick, preserving the original 32ms Extended DShot scheduler (16kHz / 512).
- `src/io.c` already excludes STM32 UART serial parser under `!AM13E`. Its shared DShot command decoder, `dshotext` flag, `dshotval` and PB14 GCR/NRZI path remain untouched.
- Do not compile `telem_mode_plan.c`, `telem_uc2_preflight.c`, or SDK `dl_unicomm.c` and `dl_unicommuart.c` into the FW1 target. Those earlier investigative sources remain in the repository for history; they are **not** FW1 deliverables.
- The five unresolved `am13e_telem_hw_*` UART symbols are **out of scope**, not to be implemented or replaced with fake no-op functions for this FW1 target.
- The remaining Motor/Protection, Audio and Config Flash symbols must still use real backends; `--no-undefined` remains enabled.
- Re-run ARM GNU Object Build and strict FW1 Link. The previous E1-AL 27-symbol / 38-reference linker log predates this PB14-only target correction; **do not claim** a reduced number until a fresh link log is captured.

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
