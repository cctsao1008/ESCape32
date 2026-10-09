# AM13E ESCape32 Rel17 Application — porting ledger

This is an **application** work log, not a board-validation report.
Boot service remains independently versioned and is not changed here.

## E1-A: platform/header isolation

Evidence: first WSL ARM GCC build log after commits
`face920`, `2f6ca1`, and `bfd1d1` (2026-10-09).

- `src/common.h` selects the TI Cortex-M33 `soc.h` under `AM13E`;
  legacy libopencm3 headers remain unchanged under `#else`.
- `mcu/AM13E/config.h` is application-only (not Boot PB14 configuration).
- Root CMake selects `mcu/AM13E` for the AM13E application includes.
- `src/defs.h` legacy TIMx dead-time encoding / comparator mapping is
  excluded for AM13E. Missing `DEAD_TIME` / `COMP_MAP` is deliberate:
  no unverified value is introduced.
- There is **no** AM13E dummy TIM register model, board clock, dead time,
  power-stage pin mapping or peripheral implementation.
- No ARM rebuild has yet been reported after the `src/defs.h` adjustment.

## E1-B WSL telemetry gate and utility isolation (2026-10-09)

Second WSL log `e1b-telemetry.log` shows `telem.c.obj` **PASS**
(no diagnostic on its compilation), alongside prior `prog.c.obj` PASS.
`util.c`, `io.c`, `main.c` remain FAIL. Verified object total: **2/5**.

To address the `util.c` diagnostics, Stage E1-B isolates:

- Legacy GPIO/Hall/LED/HSI implementations and board mapping
  within `#if !defined(AM13E)`. Board GPIO entry points must
  be supplied by the actual AM13E backend, not empty weak functions.
- Flash storage `savecfg()`: its busy/ERTM gating remains common;
  AM13E delegates erase/program/verification to the **undefined**
  `am13e_app_cfg_commit()` backend. `resetcfg()` and
  `checkcfg()` are kept shared.
- `resetcom()`: requires an actual power-stage-safe implementation.
- Music and AU PCM: keep Rel17 score parsing, audio decode and
  blocking timing contract, but isolate TIM1/TIM6 programming
  via **declarations only** in `mcu/AM13E/util_backend.h`.
- CRC8/CRC16, Scale, Smooth and PID routines remain shared.
- A static preprocessing-path check found no legacy STM32
  RCC/Flash/TIM register accesses in the AM13E `util.c` branch;
  it did not run the ARM compiler.

**The updated `util.c.obj` has not yet been compiled on WSL.**
Do not claim 3/5 PASS until its ARM GCC output is received.
No config Flash page layout, dead time, clock or safe motor
output has been fabricated. Executable linking must remain
blocked until real peripherals are implemented.

## E1-B first slice: telemetry transport isolation (2026-10-09)

Changes are committed, **WSL ARM object build not yet verified**.

- `src/telem.c` retains the shared Rel17 iBUS, S.Port, MSB, HoTT,
  KISS and CRSF encoders/decoders. The full legacy USART/DMA branches
  are retained behind `#if !defined(AM13E)`.
- New `mcu/AM13E/telem_backend.h` is a **declarations-only** transport
  contract (UART mode initialization, asynchronous TX, byte TX, RX pause,
  RX-frame and TX-complete callbacks). There is no driver implementation.
- The AM13E parser callbacks keep access to the existing `iobuf`.
  The backend must preserve half-duplex direction/turnaround,
  receive-mode rearming, asynchronous buffer lifetime and delayed HoTT
  transmission. These are not hardware-qualified.
- Static conditional-path scan: **no USART1/DMA1/STM32 register references
  active in `telem.c` with `AM13E` defined**; all six protocol paths
  remain included. This is a structural check, NOT ARM GCC PASS.
- Since `am13e_telem_hw_*` functions lack implementation intentionally,
  a future production executable must not link successfully until a real
  backend is supplied.

**Immediate gate:** rebuild AM13E objects in WSL; `telem.c.obj`
is expected to compile, but this has not been proven yet.

## First actual object-compile result (before defs.h adjustment)

| Rel17 source | ARM GCC object status | First real blocking dependency |
| --- | --- | --- |
| `src/prog.c` | PASS | No compile-blocking peripheral reference |
| `src/telem.c` | FAIL | STM32-style USART1, DMA and serial timeout |
| `src/util.c` | FAIL | RCC, Flash configuration writes, TIM1/TIM6 audio and reset |
| `src/io.c` | FAIL | Timer input capture, DMA, DShot, USART, watchdog |
| `src/main.c` | FAIL | TIM1 complementary PWM, BEMF timer, SysTick/ISR, watchdog |

The prior `prog.c` PASS is **object compilation only**; configuration
storage / all external references have not been linked or exercised.

The absent STM32-style register names are evidence that AM13E's independent
hardware boundary is working, **not** reason to add synthetic aliases.
The branch is not yet E1-B PASS.

## Dependency contracts awaiting real backends

| Service | Rel17 entry points / examples | Required AM13E backend | Hardware decisions pending |
| --- | --- | --- | --- |
| Input capture and DShot | `initio`, `iotim_isr`, `dshotirq` | Input timer/capture/DMA/interrupts | Pins, capture resolution, DShot modes |
| Telemetry transport | `inittelem`, `sendtelemdata` | UART/DMA/IRQ transport | Physical UART, pinmux, baud/inversion |
| Motor commutation | `nextstep`, `iftim_isr` | MCPWM + COMP/TIMG | Gate driver, phase and BEMF mapping |
| Emergency shutdown | `hard_fault_handler`, `resetcom` | Force outputs off + reset/watchdog | Polarity, fault line, fail-safe contract |
| Config persistence | `savecfg`, `resetcfg` | Flash parameter driver | Config partition, erasure granularity, ECC |
| Runtime timing | `sys_tick_handler`, `delay` | SysTick / IRQ priority | Clock source, scheduler tick |

## Next integration order

1. Preserve `src/prog.c` object PASS and remove the AM13E-only
   `-Wundef` warnings by isolating legacy `TIM_DTG` and `COMP_MAP`.
2. Isolate `src/telem.c` protocol encoding from its STM32 UART/DMA
   transport. AM13E transport APIs may be **declared** but must not be
   given no-op implementations to create false runtime success.
3. Repeat for `src/io.c` and `src/util.c`, with `src/main.c`
   handled as a separate commutation/PWM boundary.
4. E1-B PASS requires five meaningful ARM object files and dependency
   inventory, not simply compilation of five empty files.
5. ELF linking, board-safe outputs, firmware update, and hardware testing
   are separate later gates.

## Local verification

```bash
cmake -S . -B build-am13e -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" -- -k 0 \
    2>&1 | tee build-am13e/e1b-diagnostic.log
```

Expected at the current gate: `src/prog.c` can compile; other sources
still need hardware boundary isolation. Do not assert build success
without actual WSL results.
