# ESCape32 rel17 → TI AM13E23019 — Integration Mapping

**Revision:** v1.6  
**Scope:** All upstream Application/Boot feature families and their conditionally compiled MCU hardware seams, as available in rel17; no added or deleted application feature.  
**Related:** [Integration_Design.md](Integration_Design.md) · [Interface_Contracts.md](Interface_Contracts.md)

## How to read this mapping

Each stable ID identifies an **upstream function/module or related hardware seam**, not a source-line number. **PRESERVE** means retain the upstream policy/algorithm; **HOOK** means implement the *existing* named ESCape32 API on AM13E; **ADAPT** means make the minimal AM13E-only shared-code hardware seam call(s); **CONDITIONAL** means retain the upstream compile/runtime selection, not disable the functionality; **BUILD** means add an AM13E target without restructuring legacy targets. An entry can involve multiple actions. `Axx`/`Cxx`/`Bxx` are the private design-contract IDs in [Interface_Contracts.md](Interface_Contracts.md), not names found upstream.

The **full-feature guarantee is coverage of each source-present option and behavior through a designed AM13E route**. A single physical board need not enable mutually exclusive upstream pin/target configurations together. No entry marked CONDITIONAL is permission to delete or hard-disable the original feature.

## A. Motor control and waveform family

| ID | Upstream behavior / original function | Guard / selection | Action and retained responsibility | AM13E implementation / interface | Why adaptation is needed |
|---|---|---|---|---|---|
| M01 | `main()`, `nextstep()`, source `seq[]`, forward/reverse | Standard 6-step | PRESERVE source sequence, step selection, throttle/ramp; ADAPT physical outputs | `A01`, `A02`, `A05` → `mcu/AM13E/hal/hal_motor.c` | STM32 `TIM1_CCMR*`/`CCER` state cannot be passed to MCPWM |
| M02 | `nextstep()`: energized/floating phase, forced output states | `PWM_ENABLE`, `INVERTED_HIGH`, `cfg.damp` | ADAPT **per-phase high-/low-side mode**, not just phase masks | `A01` generalized `drive_plan`, `A05` commit | Source has different PWM1/forced-high/forced-low and enabled-channel combinations |
| M03 | Main-loop `arr`, `ccr`, dead time, variable PWM frequency | `FULL_DUTY`, brushed, `cfg.freq_min/max` | PRESERVE calculations; stage all 3 compares and period | `A02`; `hal/hal_motor.c` | Hardware clock, count direction, dead-time and duty realization differ |
| M04 | Sine startup: `nextstep()` 360-position sine lookup | `cfg.sine_range`, `cfg.sine_power`, `prep` | PRESERVE 360-position lookup and direction; ADAPT 3 independently modulated phases | `A01` sine drive plan, `A02` 3 compare channels, `A05` | v1.4 Source/Sink/Floating-only service could not express sine |
| M05 | Sine→6-step transition in main loop and `laststep()` | `sine`, `prep`, `step` | PRESERVE calculation and phase alignment | `A08` accepts source-computed deadline; `A01/A05` stage mode switch | Scheduler timer reset and COM behavior must match rel17 |
| M06 | Brushed drive path in `main()` | `cfg.brushed` | PRESERVE direction/throttle logic; ADAPT channel drive-state vector | `A01` brushed drive plan, `A02`, `A05` | Brushed state uses two driven channels/other forced output, not standard six-step |
| M07 | Idle, coast, drag/active brake, low-side short, ZTC | `cfg.duty_drag/lock/ztc`, `throt_mode` | PRESERVE state/priority; ADAPT gate states and timing | `A03` logical idle modes; `A01/A05` for active drive | Safe-off, float, sink and short are not interchangeable |
| M08 | `resetcom()`, `laststep()` | Idle and stop | PRESERVE upstream transition and lock; ADAPT forced modes | `A03`, `A05` | Previously embedded STM32 forced PWM mode encodings |
| M09 | `tim1_com_isr()` | Per-MCU `STM32G4`/`AT32F4` conditional | PRESERVE platform-dependent pre-step semantics; ADAPT COM IRQ source and ack | `C01`, `A05`, `A06`, `hal/hal_timing.c` | AM13E has no STM32 TIM1 COM interrupt/vector |
| M10 | `TIM1` update/capture IRQ for software blanking | `SW_BLANKING` | PRESERVE ordering and blanking intervals; deliver original events | `A24` event schedule, `C11` event callback | Source uses TIM1 UEV/CC4 interrupt interactions |
| M11 | `hard_fault_handler()`, motor trip | All, `LED_STAT` optional | PRESERVE output-off, error indication, source delay/restart semantics | `A04`, `A14`, retained `ledctl()` | STM32 break/watchdog registers not portable |
| M12 | Startup arming and zero-throttle 250-ms timer | `cfg.arm`, reset cause | PRESERVE arming state transitions and timeout reset | `A24` auxiliary timeout, `C11`, `A13` | TIM6 hardware wait and MCU reset cause differ |
| M13 | `beep()`, `playmusic()`, `playsound()` | Startup, beacon, CLI/music | PRESERVE note/sample parsing, busy/stop rules and phase swapping; ADAPT bridge waveform/audio timing | `A01/A02/A05`, `A24/C11` | rel17 directly reprograms TIM1/TIM6 for audio |
| M14 | Fixed/variable carrier and PWM peripheral start | `PWM_ENABLE`, `DEAD_TIME`, `FULL_DUTY` | PRESERVE original logical output polarity/duty option | `A01/A02/A05` with MCU/board gate truth table | Some options are driver-topology specific |
| M15 | Power-stage fault/off during stop and Boot entry | All | PRESERVE shutdown before reset; ADAPT hardware trip | `A04` and existing source service/reset entry | Motor output must never remain energized during transfer |

## B. BEMF, Hall, feedback and sensors

| ID | Upstream behavior / original function | Guard / selection | Action and retained responsibility | AM13E implementation / interface | Why adaptation is needed |
|---|---|---|---|---|---|
| F01 | `compctl(int)` | Native target | HOOK: preserve previous-phase `compctl(pcc); pcc=cc` selection order | Existing `compctl()` in `mcu/AM13E/config.c`, CMPSS route | Comparator selection is MCU-specific, not a new protocol |
| F02 | `nextstep()` BEMF filter and timeout-band selection | `IFTIM_*`, ERPM bands | PRESERVE conditional band calculation; ADAPT capture filter/timeout | `A07` with filter, timeout and selected route | AM13E comparator/eCAP/timer paths differ |
| F03 | `iftim_isr()` timeout/overflow | All sensorless | PRESERVE timeout-first test and `sync/fast/ival/ertm` reset | `C03`, `A09` with event-generation arbitration | On simultaneous flags, rel17 checks UIF before capture |
| F04 | `iftim_isr()` accepted/rejected zero crossing | All sensorless | PRESERVE early-reject, `fast`, `ival`, `cfg.timing` | `C02`, `A08`; backend supplies source-equivalent elapsed time | Timer reset occurs on accepted handler, not physical capture epoch |
| F05 | `tim1_com_isr()` deadline | Sensorless and sine | PRESERVE next-step event behavior | `A06/A08`, `C01`; source `nextstep()` | AM13E does not supply STM32 COM interrupt |
| F06 | `getcode()`, `hallcode()` | `HALL_MAP`, `USE_XOR` | PRESERVE Hall code checking; HOOK `hallcode()`; ADAPT pin decode | `mcu/AM13E/config.c`, `A18` if pin sampling needed | Multi-pin/EXOR input route depends on MCU/board |
| F07 | `tim3_isr()` Hall edge and timeout, hybrid mode | `HALL_MAP` | PRESERVE `hall`, `ival`, `sync` and timeout policy; ADAPT Hall capture | `A19`, `C06`, `hal/hal_hall.c` | A comparator BEMF ISR is not equivalent to a Hall timer |
| F08 | `initgpio()` Hall port configuration | `HALL_MAP` and associated port guards | HOOK/ADAPT pinmux/pulls in MCU; preserve source Hall decoding | `config.c`, `A18`, existing `hallcode()` | No STM32 GPIO register in common AM13E build |
| F09 | `adctrig()` / ADC DMA callbacks | `SENS_MAP`, `TEMP_*`, `ANALOG_CHAN` | HOOK: preserve sampling/trigger cadence | `adctrig()`, `hal/hal_adc.c` | TI ADC/trigger/DMA differ |
| F10 | `adcdata(int t,int u,int v,int c,int a)` | `SENS_CNT`, `TEMP_ADC2`, source sensor guards | PRESERVE argument order and protection/telemetry calculations | Existing `adcdata()`, sensor implementation in `hal/hal_adc.c` | No new measurement semantics beyond source |
| F11 | Optional comparator/OPAMP implementation | `USE_COMP2`, `USE_OPAMP`, MCU config | PRESERVE selected comparator input topology, not its STM32 register encoding | `compctl()`, native analog routing in `config.c` | Conditional board analog paths require explicit native mux mapping |
| F12 | Analog input path | `ANALOG`, `ANALOG_CHAN`, `IO_ANALOG` | HOOK existing `io_analog()`, retain throttle transfer | `hal/hal_adc.c` + existing `io_analog()` | ADC source switching, not a new input feature |

## C. Receiver, protocol and command transport

| ID | Upstream behavior / original function | Guard / selection | Action and retained responsibility | AM13E implementation / interface | Why adaptation is needed |
|---|---|---|---|---|---|
| I01 | `initio()`, `entryirq()` pulse/timeout discriminator | `INPUT_MODE`, `IO_PA2`, `IO_PA6`, analog fallback | PRESERVE original mode selection, CLI entry, watchdog start | `A20`, `C07`, native receiver capture in `hal/hal_input.c` | Timer edge/reset and IDR are MCU-specific |
| I02 | `calibirq()`, `servoirq()` | Servo PWM / Oneshot | PRESERVE source pulse calibration, special telemetry request and throttle/failsafe | `C05`, `A20`, native capture | Must deliver period and width in rel17 units |
| I03 | `dshotirq()`, `dshotreset()`, `dshotresync()` | DShot receiver | PRESERVE bad-sync handling, first/next capture, reset/re-arm | `A20`, `C07`, `hal/hal_input.c` | Source controls DMA/timer rearm at distinct states |
| I04 | `iotim_dma_isr()`, `dshotcrc()` | DShot, inverted DShot | PRESERVE CRC/commands/reply pipeline; hardware captures **32** source-style interval samples | `C04` plus `A20`; `hal/hal_input.c` | Do not move DShot parser into HAL |
| I05 | Inverted DShot same-line RX→TX→RX, GCR data | `dshotinv`, DShot Extended Telemetry | PRESERVE upstream GCR encoding, `dshotval`, response ordering | `A16` prepared **23-word compare waveform**, `C04`, `A21`; `hal/hal_input.c` | v1.4 `encoded_gcr20` API would duplicate/move source GCR encoding |
| I06 | `setthrot()`, `setbrake()` | `cfg.throt_mode`, `IO_AUX` | PRESERVE upstream throttle/braking math | Existing shared functions, `C10` for AUX event | Physical edge capture only |
| I07 | `iotim2_isr()` AUX brake pulse | `IO_AUX` | PRESERVE pulse-to-brake handling; ADAPT second capture and edge toggle | `A20`, `C10`; `hal/hal_input.c` | Source uses TIM16/17 or dual-channel timer route |
| I08 | `serialirq()`, `serialfunc()` | `IO_PA2`, `INPUT_MODE=2` | PRESERVE buffer/command decoder; ADAPT UART/DMA | `A21`, `C08`; `hal/hal_input.c`/`hal_serial.c` | UART idle, half-duplex and DMA ownership differ |
| I09 | `ibusfunc()` | `INPUT_MODE=3` | PRESERVE checksum/channel extraction, optional AUX brake | `A21`, `C08` | No new iBUS parser |
| I10 | `sbusirq1()`, `sbusirq2()`, `sbusfunc()` | `INPUT_MODE=4`, SBUS/SBUS2 | PRESERVE parity, inverted waveform, TX slot schedule and channel extraction | `A21`, `A24`, `C08/C11` | A UART-only byte callback would lose the telemetry-slot transition |
| I11 | `crsffunc()`, CRSF command response | `INPUT_MODE=5`, `SMALL_DEVICE` guards | PRESERVE CRC, channel and `execcrsfcmd()` behavior | `A21`, `C08`, existing `sendtelemdata()` | Source protocol remains unchanged |
| I12 | `exbusfunc()`, `exbusresp()` | `INPUT_MODE=6`, `!DISABLE_EXBUS` | PRESERVE CRC, baud fallback, replies | `A21`, `C08` | Response/baud transitions are backend actions |
| I13 | `hottfunc()` | `INPUT_MODE=7`, `!DISABLE_HOTT` | PRESERVE SUMDv1/v3 decode/failsafe | `A21`, `C08` | Serial configuration/mode selection varies |
| I14 | CLI `cliirq()` USART path | `IO_PA2` | PRESERVE line editing, `execcmd()` and boot-entry sequence | `A21`, `C08`, native 38400 baud | Cannot replace parser with new CLI |
| I15 | CLI `cliirq()` timer bit-bang path | `!IO_PA2` | PRESERVE start/data/stop bit framing, response send and watchdog-error behavior | `A21` soft-UART mode, `A24/C11` timing | A physical UART is not present in this upstream conditional path |
| I16 | `io_serial()`, `io_analog()` | Selected by `entryirq()` or fixed analog input | HOOK existing names in AM13E MCU config | `config.c`, relevant native modes `A21` | Avoid inventing redundant initialization APIs |
| I17 | I/O timeout, fatal input framing, `scb_reset_system()` | All applicable input modes | PRESERVE reset/failsafe policy and motor shutdown | `A04`, `A14`/native reset, `A24` | Watchdog/WWDG behavior not STM32-compatible hardware |
| I18 | Extended DShot telemetry values/command switch | DShot command 13/14, telemetry setting | PRESERVE `dshotext`, `dshotval`, `rep` and packet generation | Existing shared `src/io.c`/`src/telem.c`, `A16` native TX | No extra Host service or changed reply timing |

## D. Telemetry, CLI, auxiliary functions and configuration

| ID | Upstream behavior / original function | Guard / selection | Action and retained responsibility | AM13E implementation / interface | Why adaptation is needed |
|---|---|---|---|---|---|
| T01 | `inittelem()`, `usart1_isr()` | `cfg.telem_mode` 0–6, DISABLE guards | PRESERVE protocol switch and send/receive state | `A22/A23`, `C09`, `hal/hal_telem.c` | Half-duplex UART idle/timeout/TC + DMA differ |
| T02 | `usart1_tx_dma_isr()`, `sendtelemdata()` | Buffered telemetry | PRESERVE queue head/tail and refill order | `A23`, `C09`; TX complete + DMA generation | DMA can only read committed bytes |
| T03 | `sendtelem()`, `sendkiss()`, `sendcrsf()` | `telreq`, modes 0/1/4 | PRESERVE tick cadence and packet bytes | Shared functions; `A22/A23` native output | No new encoder |
| T04 | `ibusfunc()`, `sportfunc()`, `msbfunc()`, `hottfunc()` | Modes 2,3,5,6 / `DISABLE_MSB/HOTT` | PRESERVE CRC, request-reply timing, byte-stuffing and telemetry units | `A22/A23`, `C09` | Half-duplex turnaround requires explicit ownership |
| T05 | `execcmd()`, `execcrsfcmd()` | CLI and CRSF service | PRESERVE upstream parsing and settings semantics | No new HAL; native transport `A21/A23` | Commands are not TI hardware drivers |
| T06 | `checkcfg()`, default `cfgdata`, `.cfg` copy | Startup and `resetcfg()` | PRESERVE Cfg field layout, defaults, validation and `id=0x32EA` | Existing source; Linker places Config in `[0x4000,0x5000)` | Only storage addresses change |
| T07 | `savecfg()`, `resetcfg()` | Motor stopped, `busy` guard | PRESERVE rejection/default behavior; store exact Cfg bytes | `A15`, `hal/hal_storage.c` | Flash RMW/ECC and Bank0 execution require native method |
| T08 | `initgpio()`, BEC control | `BEC_MAP`, `BEC_MIN/MAX` | PRESERVE `cfg.bec` selection, optional SWD-pin override condition | `A17/A18`, `config.c` GPIO role mapping | Port aliases and GPIO register writes are STM32-specific |
| T09 | `initgpio()`, ERPM output in `nextstep()` | `ERPM_PIN` | PRESERVE step 1/4 and sine step 1/181 toggle polarity and timing | `A17`, `hal/hal_aux.c` | Replaces direct `BSRR` writes without moving step policy |
| T10 | `park()` state machine | `PARK_PIN` | PRESERVE 800-cycle debounce/state/reverse behavior | `A18`, `hal/hal_aux.c` | Existing function reads GPIO IDR inline |
| T11 | `initled()`, `ledctl()` and status LED | `LED_MAP`, `LED_INV/OD`, `LED_STAT` | HOOK existing weak `initled/ledctl` or implement native driver; retain source LED bit selection | `mcu/AM13E/config.c` or `hal/hal_aux.c` | STM32 GPIO/open-drain polarity not portable |
| T12 | WS2812 LED source option | `LED_WS2812` | HOOK source's `ledctl()` behavior and output encoding; target timing implemented natively | Existing `ledctl()` → `hal/hal_aux.c` | One-wire waveform timing, not a new LED function |
| T13 | `hsictl(int)` in pulse calibration | `cfg.throt_cal`, clock settings | HOOK existing weak `hsictl()` with effective AM13E trim/clock control preserving pulse calibration feedback | `mcu/AM13E/config.c` | STM32 HSI trim bit fields are not universal |
| T14 | `playmusic()`, `playsound()`, `beep()` | Startup/CLI/beacon | PRESERVE note and AU sample parsers and `busy` state | `A01/A02/A05`, `A24/C11`, `hal_audio.c` | Direct TIM1 phase audio and TIM6 sample intervals need adapter |
| T15 | `initled()`, `ledctl()`, fault indication | `LED_STAT` | PRESERVE rel17 `led` state and error selection | Native existing Hooks, `A04/A14` | No extra status feature |
| T16 | Voltage/current/temperature sensors and protection | `SENS_CNT`, `TEMP_*`, `cfg.prot_*` | PRESERVE smoothing, PID, limit/protection policy | `adctrig()` and `adcdata()` existing Hooks; `hal/hal_adc.c` | No extra measurements; source sensor guards apply |
| T17 | Device unique ID in EXBUS/identity | `DESIG_UNIQUE_ID0` | PRESERVE packet bytes/field length with authentic AM13E identifier derived by native provider | `A25` native unique-ID provider in `hal_serial.c` | STM32 factory ID register is not portable |
| T18 | `sys_tick_handler()`, `pend_sv_handler()`, `delay()` | All applicable targets | PRESERVE tick/PendSV ordering and source 16kHz start point | `A10`, existing callbacks into source; `hal/hal_system.c` | M33 vector/priority mapping |
| T19 | `IWDG` start/feed, `WWDG` fault reset | Input/CLI/arming | PRESERVE watchdog start/feed/error response | `A11/A12/A14`; `hal/hal_system.c` | MCU registers and watchdog mode differ |
| T20 | Reset flags, Boot entry and software reset | Reset/service/arming | PRESERVE rel17 reset-conditional behavior | `A13/A14`, native reset, `A04` safe-off | M33 reset flags are not STM32 RCC CSR |

## E. Bootloader: original six commands and complete hardware routes

| ID | Original source behavior | Action and preserve rule | AM13E route | Flash-layout-only adaptation |
|---|---|---|---|---|
| BTM01 | `boot/src/main.c` `CMD_PROBE=0` | PRESERVE complement-coded `RES_OK` | Original `recvval/sendval`, native Boot transport | None |
| BTM02 | `CMD_INFO=1` | PRESERVE 32-byte `senddata()` reply, Boot revision/IO identity field positions | `B03`, Boot native `IO_PIN` constant | Genuine AM13E Device ID; Host detects extension via identity |
| BTM03 | `CMD_READ=2` | PRESERVE one-byte block+word count, `senddata()` CRC response, including Config-prefix read | `B01` after `CMD_WINDOW` + config-first translation | Blocks 0–3 Config; 4–491 App; Reserved skipped |
| BTM04 | `CMD_WRITE=3` | PRESERVE `recvdata()`, original `write()` status, block length/ACK semantics, **Config-prefix write** | `B02` and existing `write()` Boot hook | Bounded sector RMW + 16-byte program; config-first translation |
| BTM05 | `CMD_UPDATE=4` | PRESERVE RAM staging, per-1KiB chunk intermediate `RES_OK`, Boot-only `update()`, reset on success | Existing `update()` in `boot/mcu/AM13E/hal/hal_flash.c` | Maximum Boot image 16 KiB, RAM execute same-bank programming |
| BTM06 | `CMD_SETWRP=5` | PRESERVE Off (`0x33`), Boot (`0x44`), Full (`0x55`) protection modes and reset/reload behavior | Existing `setwrp()` in `hal/hal_protection.c` | Map 16KiB Boot span; retain reversible protection |
| BTM07 | `CMD_WINDOW=6` | ADD only, do not override original command/packet format; complement-coded window `0/1` and status | Boot parser minimal extension, `B01/B02` | Cover all 492 logical 1KiB blocks |
| BTM08 | Unknown Command / timeout → Boot exit logic | PRESERVE fallback to source marker/jump when Config marker valid | `B04/B05` Boot vector jump | Relocated `Cfg.id` at CFG_BASE, App vector at APP_BASE |
| BTM09 | `boot/src/io.c` `initio()`, `recvbuf()`, `sendbuf()` | HOOK existing names; preserve selected UART/soft-UART, timer timeout and half-duplex behavior | `boot/mcu/AM13E/hal/hal_uart.c`, native Boot timer | TI UART and timer are not STM32 TIM3/USART2 |
| BTM10 | `recvval/sendval`, `recvdata/senddata()` | PRESERVE complement pairs and CRC32 message framing | Reuse original `boot/src/io.c` logic | None |
| BTM11 | `crc32()` | PRESERVE exact polynomial/reverse/complement output for host | Existing `crc32()` with SW or valid TI CRC implementation | Avoid STM32 CRC peripheral dependency |
| BTM12 | `write()`, `update()`, `setwrp()` | HOOK existing signatures and success/error/reset semantics; do not create new public aliases | `boot/mcu/AM13E/` Flash/protection code | RAM command path, sector/ecc/protection rules |
| BTM13 | Boot reset cause and reboot ACK | PRESERVE ACK after recognized software/option reset | `B06` signature matches `A13`; native reset flags | MCU-specific reset sources |
| BTM14 | Boot vector/stack, `main()`/Startup ABI | PRESERVE direct Application jump, Cortex-M33 vector ownership | `B04/B05`, AM13E entry wrapper | APP_BASE = `0x6000` |
| BTM15 | `IO_PA2`, `IO_PA6`, `IO_RXTX`, `FAST_EXIT` | PRESERVE existing Boot target conditional transport/entry behaviors | Native Boot mode/IO config + existing Hooks | Physical pin mapping target/board-defined |

### Exact Flash logical translation used by `BTM03`/`BTM04`

| Window | Effective blocks | Physical target | Status |
|---|---|---|---|
| `0` | `0–3` | `CFG_BASE + 1024 × n` | Original Config-prefix read/write retained |
| `0` | `4–255` | `APP_BASE + 1024 × (n−4)` | Application range |
| `1` | `256–491` | `APP_BASE + 1024 × (n−4)` | Application range |
| `1` | `492–511` | None | Reject before any Flash unlock or access |

The Reserved region `[0x5000,0x6000)` is **not** part of the logical stream. The whole logical 492 KiB range is addressable by `CMD_WINDOW` + original `CMD_READ/WRITE`. The physical Application is *at most 488 KiB*: normal host uploads only the actual image size, and programming never assumes an image fills the region. If a Host previously sent a combined Config+Application stream with a different Config-prefix length, its packaging must be updated to 4 KiB; preserving command IDs alone does not fix image-layout differences.

**Protection (`BTM06`) compatibility is normative, not yet silicon-proven:** `setwrp(0)` reversibly allows updates, `setwrp(1)` protects Boot while allowing Config+App, `setwrp(2)` protects MAIN Flash. No irreversible lock may be substituted for Off. A concrete TI NONMAIN/protection implementation is required; until supported target configuration is established, this is an implementation-feasibility dependency, not permission to delete `CMD_SETWRP` or pretend successful operation.

## F. Build, runtime isolation and variant coverage

| ID | Source/build element | Action | AM13E responsibility |
|---|---|---|---|
| SYS01 | Root CMake `add_target()` and `file(GLOB srcs src/*.c mcu/${mcu}/*.c)` | BUILD AM13E-only branch: retain original root `.c` selection, explicitly add `mcu/AM13E/hal/*.c`; preserve all legacy targets | Cortex-M33 flags, TI SDK/CMSIS libraries, correct 512 KiB geometry, Startup/Vector, linker and programmer; one existing `add_target()` function |
| SYS02 | `boot/CMakeLists.txt`, Boot `add_target()` | BUILD native `AM13E` target; reuse root-defined `add_target()`, selecting `boot/mcu/AM13E/hal/*.c` from the Boot invocation, not Application `hal/` | Existing Boot target naming/output conventions; separate Boot HAL and image startup/linker |
| SYS03 | `mcu/STM32G431/{config.c,config.h,config.cmake,config.ld,preset.c}` → `mcu/AM13E/` **same five names** + private `hal/` | ADD | `config.c` original MCU Hook entry; `config.h` target definitions; `config.cmake` TI SDK/target flags; `config.ld` memory regions; `preset.c` upstream `sinedata[]` contract; `hal/` for Application-only hardware. Do **not** copy STM32 registers. |
| SYS04 | `boot/mcu/STM32G4/{config.c,config.h,config.cmake,config.ld}` → `boot/mcu/AM13E/` **same four names** + private `hal/` | ADD | Boot-specific `init()`/MCU setup, UART/Flash/protection/entry hardware under Boot `hal/`, build settings and Boot memory. No Boot `preset.c` and no Application HAL dependency. |
| SYS05 | `src/common.h` / `boot/src/common.h` vendor includes | ADAPT with strictly AM13E-guarded include and interface seam | No libopencm3/ST register types or CMSIS M3 leak into AM13E |
| SYS06 | `void main(void)` both firmware images | BUILD AM13E entry wrapper | Exactly one M33 Startup/Vector and linker map per image |
| SYS07 | Internal vectors: COM, eCAP, SysTick, PendSV, Hall, input, ADC, UART, DMA, error | BUILD native vector aliases/wrappers | Preserve function-level event order and priority relations |
| SYS08 | Flash layout in both linker maps | BUILD shared symbolic partition | BOOT 16KiB; CFG 4KiB; RESERVED 4KiB; APP max488KiB; 2KiB sectors |
| SYS09 | Target feature defines and board pin map | BUILD source-defined `HALL_MAP`, `COMP_MAP`, `SENS_MAP`, `PWM_ENABLE`, `INVERTED_HIGH`, `SW_BLANKING`, `LED_*`, `IO_*`, `BEC_*`, `PARK_*`, `ERPM_*`, `USE_*`, telemetry and input macros | Preserve original conditional branches; don't claim all board options active at once |
| SYS10 | STM32/AT32/GD32 source + output builds | PRESERVE untouched native targets | AM13E-specific adaptation never forces a global platform refactor |

### Native MCU file-to-hook ownership (part of SYS01/SYS03/SYS04, not additional API calls)

| Upstream file/function reference | AM13E file owner | Mapping/contract coverage | Reason for keeping this file boundary |
|---|---|---|---|
| STM32G431 `config.c`: `init()`, `compctl()`, `adctrig()`, `adcdma()` | `mcu/AM13E/config.c` (may delegate to `hal/hal_bemf.c`, `hal/hal_adc.c`, `hal/hal_system.c`) | F01, F09–F11, T18, A07/A10; original Hooks | Existing MCU initialization/comparator/ADC entry names are maintained; low-level STM32 registers are not. |
| STM32G431 `config.c`: `io_serial()`, `io_analog()`, `initled()`, `ledctl()`, `hsictl()` | `mcu/AM13E/config.c` (delegating to `hal/hal_input.c`, `hal/hal_aux.c`, `hal/hal_system.c` as needed) | I16, F12, T11–T13, A17/A18/A21; original Hooks | Keep rel17 source-visible platform hooks without redundant public HAL wrappers. |
| STM32G431 `config.h` | `mcu/AM13E/config.h` | SYS05, SYS07, SYS09 | Provide native target definitions and selected feature/pin roles, not fake `TIM1_*` registers. |
| STM32G431 `config.cmake` | `mcu/AM13E/config.cmake` | SYS01, SYS03 | Keep `add_target(name mcu ...)` convention; AM13E compiler/SDK/startup settings are target-local. |
| STM32G431 `config.ld` | `mcu/AM13E/config.ld` | SYS08, BTM14 | Relocate original memory symbols to the adopted AM13E Flash layout; enforce maximum, not fixed, Application size. |
| STM32G431 `preset.c`: `sinedata[]` | `mcu/AM13E/preset.c` | M02, M05–M07, T14; original symbol | Preserve the original sine table contract instead of treating it as optional board configuration or moving algorithm data into HAL. |
| STM32G4 Boot `config.c` / `config.h` | `boot/mcu/AM13E/config.c` / `config.h` | BTM09–BTM15, B03–B06; original Boot `init()` | Keep Boot MCU Hook/target definitions separate from Application. |
| STM32G4 Boot `config.cmake` / `config.ld` | `boot/mcu/AM13E/config.cmake` / `config.ld` | SYS02, SYS04, SYS08 | Keep original Boot target declaration/build layout and independent Boot memory/entry. |
| Existing shared-source direct hardware operations | Candidate `mcu/AM13E/hal/hal_*.c` or `boot/mcu/AM13E/hal/hal_*.c`, invoked through the smallest native seam | M01–M14, F01–F12, I01–I17, T01–T19, BTM01–BTM15 | Keep policies in shared Core; segregate each image's private hardware implementation. Candidate file count is not prescribed. |

The five Application and four Boot filenames are **required organizational counterparts**, whereas each `hal/hal_*.c` filename is a **candidate implementation split**. A caller mapped to `hal/hal_bemf.c` still enters via the existing `compctl()` Hook where that Hook suffices; a HAL module is not automatically a new external API. **Application and Boot each own a nested `hal/` directory.** The unchanged upstream top-level `file(GLOB ...)` does not recurse; an AM13E-only addition to the existing root `add_target()` function must select `mcu/AM13E/hal/*.c` for the Application and `boot/mcu/AM13E/hal/*.c` for Boot through their respective CMake invocation directories. No source-code line-number inventory or separate change-plan file is required.

## G. Mapping completeness and interface selection

The private interface IDs are **design seams**, not public ESCape32 API changes: `A01–A25` and `C01–C11` cover Application motor, input, telemetry, auxiliaries and events; `B01–B05` cover Boot Flash/identity/launch. Boot `B06` uses the same function signature as Application `A13` in a separately linked image. **41 unique new private function names** are documented; existing source Hooks are not included in that count. Several source features share one private interface. Count is derived from explicitly described hardware seams, not a fixed target or an assertion that exactly 41 C functions must appear in final firmware.

Unlike v1.4, this mapping does not leave sine/brushed/Hall/audio/serial soft-UART/auxiliary functions at merely “backend supports it”: each has a concrete service or existing Hook, an event source and ownership. No C implementation was created. Actual pinmux/capacity limits and selected board capability are target configuration inputs. No toolchain or physical measurements are counted as design completion.

## H. rel17 compile-time/runtime option cross-check (source-derived groups)

This is an **in-document coverage index**, not a separate evidence file and not a proposed set of new features. Existing `defs.h`, `config.h`, `config.cmake`, `mcu/*/config.c` and board `add_target()` definitions are authoritative. Runtime values remain subject to upstream `checkcfg()` limits. `#if` variants that refer to older STM32 channel names must be **translated by role**, not redefined into fake STM32 peripherals.

| Existing selection family | Representative rel17 names (source exact) | Mapping / native responsibility |
|---|---|---|
| Motor defaults / dynamics | `ARM`, `DAMP`, `REVDIR`, `BRUSHED`, `TIMING`, `SINE_RANGE`, `SINE_POWER`, `FREQ_MIN`, `FREQ_MAX`, `DUTY_MIN`, `DUTY_MAX`, `DUTY_SPUP`, `DUTY_RAMP`, `DUTY_RATE`, `DUTY_DRAG`, `DUTY_LOCK` | M01–M14: unchanged source calculations; drive plan, duty timing and sound |
| Motor output target variants | `PWM_ENABLE`, `INVERTED_HIGH`, `FULL_DUTY`, `DEAD_TIME`, `SW_BLANKING`, `TIM1_CCR5` | M02–M04, M09–M10, M14: per-channel mode, blanking/event/OC routing |
| Throttle/direction/input values | `THROT_MODE`, `THROT_ZTC`, `THROT_REV`, `THROT_BRK`, `THROT_SET`, `THROT_CAL`, `THROT_MIN`, `THROT_MID`, `THROT_MAX`, `ANALOG_MIN`, `ANALOG_MAX` | I01–I02, I06, F12, T13: keep rel17 normalization/calibration and watchdog |
| Receiver selection | `INPUT_MODE`, `INPUT_CH1`, `INPUT_CH2`, `IO_PA2`, `IO_PA6`, `IO_RXTX`, `IO_AUX`, `IOTIM2`, `TIM16`, `TIM17`, `SERIAL_BR` | I01–I17: peripheral input/dual timer/serial transport roles |
| Input protocol conditional exclusions | `SMALL_DEVICE`, `DISABLE_EXBUS`, `DISABLE_HOTT` | I10–I13: preserve source guard semantics rather than rewriting protocols |
| Telemetry | `TELEM_MODE`, `TELEM_PHID`, `TELEM_POLES`, `TELEM_VOLT`, `TELEM_CURR`, `DISABLE_MSB`, `DISABLE_HOTT`, `USART1_DMA_BASE` | T01–T04: original formats and DMA/half-duplex queue |
| Sensor/protection | `SENS_MAP`, `SENS_CNT`, `TEMP_SENS`, `PROT_STALL`, `PROT_TEMP`, `PROT_SENS`, `PROT_VOLT`, `PROT_CELLS`, `PROT_CURR`, `PROT_PARK`, `ANALOG`, `ANALOG_CHAN` | F09–F12, T16, T07: preserve sensor callback values and protection rules |
| Analog/filter target variants | `COMP_MAP`, `USE_COMP2`, `USE_OPAMP`, `USE_XOR`, `USE_HSE` | F01–F08, T13: native comparator/clock/Hall assignment |
| Hall physical aliases | `HALL_MAP`, `HALL1_PORT`, `HALL1_PIN2`, `HALL2_PIN2`, `HALL2_PIN3`, `HALL_PIN2`, `HALL_PIN3` | F06–F08: multi-port Hall code and capture |
| Status/LED | `LED_MAP`, `LED_WS2812`, `LED_STAT`, `LED_CNT`, `LED_INV`, `LED_OD`, `LED1_INV`, `LED2_INV`, `LED3_INV`, `LED4_INV`, `LED1_OD`, `LED2_OD`, `LED3_OD`, `LED4_OD`, per-LED port aliases | T11–T12, T15: existing weak LED hooks + native board implementation |
| BEC/parking/ERPM | `BEC_MAP`, `BEC_MIN`, `BEC_MAX`, `BEC_PIN2`, `BEC_PIN3`, `PARK_PIN`, `PARK_PORT`, `ERPM_PIN`, `ERPM_PORT` | T08–T10, M13: role-based GPIO with upstream logic preserved |
| Beep/startup melody | `BEACON`, `MUSIC`, `VOLUME`, `cfg.music`, `cfg.volume`, `beepval` | M13/T14: source sound parsers and timers |
| Legacy-vendor Flash compatibility | `FLASH_CR_STRT`, `STM32F0` / STM32-family guards | BTM03–BTM12 and T07: replace target register operations, not source Flash-feature intent |

The values of `COMP_MAP`, `SENS_MAP`, `LED_MAP` and the Hall/port macros are **board-specific wiring descriptions**, not independent product requirements. The AM13E port must preserve the ability to express the original mapping variants via native board configuration and route ownership. Where a particular board cannot physically implement a selected combination, that board target cannot claim such a configuration; **the overall port shall not delete the upstream feature family**.
