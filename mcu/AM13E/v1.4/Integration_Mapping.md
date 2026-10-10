# ESCape32 rel17 to AM13E23019 — Integration Mapping

**Revision:** 1.4, full upstream rel17 feature preservation and additive Boot command extension. Source identifiers are **module/function names**, not line numbers.  
**Authority:** [Integration_Design.md](Integration_Design.md); exact interfaces in [Interface_Contracts.md](Interface_Contracts.md).  
**Scope:** ESCape32 rel17 sensorless six-step Application and ESCape32-derived Bootloader integration; no unrelated motor-control implementation is specified.

**Action labels:** **PRESERVE** upstream algorithm/protocol; **REUSE** existing ESCape32 hook; **ISOLATE** only MCU-specific register/transport behavior; **ADD** MCU-local backend; **CONDITIONAL UPSTREAM** original feature enabled by rel17 runtime/compile/board configuration, not deleted or made mandatory; **BOARD-DEPENDENT** requires actual target wiring/scale.

No new top-level `platforms/` tree, independent HAL Framework or old 99-API requirement. Preserve original legacy MCU branches, both `add_target()` flows, **every original Boot command and its full operation**. Selected board feature guards remain conditional, not deleted.

## 1. ESCape32 Application — motor and BEMF

| ID | Existing source / function | Action | Interface / separation | Owner | Reason |
|---|---|---|---|---|---|
| APP-M01 | `src/main.c` / `nextstep()` phase calculation | PRESERVE | Existing `seq[]`, `step`, `p`, `n`, `m`, `cc`, `reverse` | Shared `src/main.c` | Six-step algorithm is not STM32-specific |
| APP-M02 | `nextstep()` / `TIM1_CCMR*`, `TIM1_CCER` | ISOLATE | `A01` stage logical source/sink/float, `A05` commit where legacy COM event is required | `hal_motor.c` (MCPWM) | Timer register encoding is not portable; preserve gated update ordering |
| APP-M03 | Main loop / `TIM1_ARR`, `TIM1_CCR1..3`, carrier frequency | ISOLATE | `A02` | `hal_motor.c` | Keep existing `scale()`/slew/frequency calculation; replace only timer programming |
| APP-M04 | `nextstep()` / `compctl(pcc); pcc=cc` | REUSE | Existing `void compctl(int)` | `config.c` / `hal_bemf.c` (CMPSS) | A pre-existing MCU hook already implements the delayed floating-phase comparator selection |
| APP-M05 | `nextstep()` / IFTIM capture filter, event clear, timeout setup | ISOLATE | `A07` | `hal_bemf.c` (eCAP/timer) | Capture and timeout hardware differ; selected BEMF route remains `compctl()`'s responsibility |
| APP-M06 | `iftim_isr()` / capture case | ISOLATE | `C02` returns accepted/rejected; the accepted shared formula calls `A08` | `hal_bemf.c` invokes shared handler | Keep `t < ival/2`, fast-accel detection, `ival`, `cfg.timing` and timer-reset epoch |
| APP-M07 | `iftim_isr()` / overflow case | ISOLATE | `C03`, plus `A09` to disable capture/deadline | `hal_bemf.c` | Timeout must win over capture when both are pending, with original `sync/fast/ival/ertm` response |
| APP-M08 | `tim1_com_isr()` / COM event | ISOLATE | `C01` logical due callback; `A06` enable/disable, `A05` commit | `hal_motor.c` / `hal_bemf.c` IRQ dispatch | Legacy ISR name/flags do not exist on AM13E; keep `nextstep()` dispatch policy |
| APP-M09 | `src/main.c` / motor startup and stop sequence | ISOLATE | `A01`, `A03`, `A05`, `A06`, `A07`, `A09` | Motor + BEMF adapters | Keep state transition, synchronization and PWM duty logic; do not copy TIM event generation |
| APP-M10 | `laststep()` / step completion and lock | ISOLATE | `A03`/`A05`, existing `resetcom()` logic | `hal_motor.c` | Idle/short output is distinct from fault safe-off |
| APP-M11 | `src/util.c` / `resetcom()` | ISOLATE | `A03` with defined idle mode | `hal_motor.c` | STM32 forced-output/CCER pattern is not a physical gate-driver truth table |
| APP-M12 | `src/main.c` / emergency trip in `hard_fault_handler()` | ISOLATE | `A04`, then `A14` | `hal_motor.c`, `hal_system.c` | Safe gate-off must not depend on TIM1 break bit or STM32 watchdog register |
| APP-M13 | `nextstep()` sine branch / `sinedata` and TIM1 carrier mode | CONDITIONAL UPSTREAM | Preserve `cfg.sine_range`/sine PWM source path; native three-phase PWM adaptation is needed when enabled | AM13E motor backend | Present in rel17; cannot be substituted with the six-step phase-role adapter |
| APP-M14 | Main loop brushed branch / three output mode combinations | CONDITIONAL UPSTREAM | Preserve `cfg.brushed` behavior; native brushed output-vector mapping when enabled | AM13E motor backend | Present in rel17 and not equivalent to the six-step primitive |
| APP-M15 | Main loop `cfg.damp`/`cfg.duty_lock`/zero-throttle states | PRESERVE + HARDWARE ADAPTATION | `A01` damp flag; `A03` idle mode; hardware-adapt all source-defined states; safe-off on invalid requests | Shared config policy + `hal_motor.c` | Avoid rebranding complementary PWM, short, brake and coast as identical |
| APP-M16 | Hall `getcode()` / `tim3_isr()` and `hallcode()` | CONDITIONAL UPSTREAM | Existing `hallcode()`; native Hall GPIO/capture/timeout adapter if `HALL_MAP` selected | MCU `config.c` / conditional input backend | rel17 hybrid/Hall path is real source functionality; depends on board signals |
| APP-M17 | Parking / `park()` / `PARK_PIN`, ERPM output | CONDITIONAL UPSTREAM | Preserve original `PARK_PIN`/`ERPM_PIN` guards; native GPIO adapter when selected | MCU `config.c` / conditional GPIO | Source-defined feature, not an external port mandate |

**Motor sequencing invariant:** preserve `seq[]` source/sink/floating selection, `compctl(pcc); pcc=cc`, rel17 early-ZC rejection and capture/timeout precedence, and the ISR-handler timer-reset epoch before next commutation scheduling. BEMF analog pin selection and fast overcurrent protection must have nonconflicting board-level ownership. No STM32 TIM register encoding is a public HAL contract.

## 2. ESCape32 Application — input, protocol, sensing, persistence and service

| ID | Existing source / function | Action | Interface / separation | Owner | Reason |
|---|---|---|---|---|---|
| APP-I01 | `src/io.c` / `initio()` | ISOLATE | Reuse rel17 `initio()` input-capture/automatic input-path selection; native timer/DMA backend handles its selected mode(s) | `mcu/AM13E/hal_input.c` | Preserve upstream behavior; no external requirement dictates enabled protocols or a particular pin |
| APP-I02 | `entryirq()`, `calibirq()`, `servoirq()`, `dshotirq()`, `dshotreset()`, `dshotresync()` | ISOLATE | Reuse rel17 input identification, pulse validation, capture-reset and resynchronization; native backend owns MCU event/capture resources | `hal_input.c` + shared `src/io.c` | All are already present in rel17; no newly invented receiver policy |
| APP-I03 | `iotim_dma_isr()`, DShot parser | ISOLATE | `C04` delivers rel17-style captured intervals/polarity; `A16` provides MCU waveform primitive for upstream inverted mode; retain RX completion → TX start → frame decode → TX completion → response preparation ordering | `hal_input.c` + shared `src/io.c` | Bidirectional operation is implemented in rel17; protocol ordering comes from source, not a requirement imported from elsewhere |
| APP-I04 | `dshotcrc()`, command switch, `dshotval` | PRESERVE + ISOLATE | Retain rel17 checksum/commands, inverted mode, GCR encoding and telemetry pipeline exactly where compiled; no new command or telemetry policy | shared `src/io.c`, `hal_input.c` | Source behavior, not an externally mandated telemetry capability |
| APP-I05 | `setthrot()` and optional `setbrake()` | PRESERVE | No new HAL API | `src/io.c` | Throttle mapping is ESCape32 policy |
| APP-I06 | `calibirq()`, `servoirq()`, `setthrot()` | ISOLATE | `C05` supplies measured source-style pulse and period where servo/Oneshot mode is selected; preserve original throttle and timing rules | `hal_input.c` + shared `src/io.c` | Servo/Oneshot are upstream rel17 modes |
| APP-I07 | `serialirq()`, SBUS/iBUS/CRSF/EXBUS/HoTT, CLI handlers | CONDITIONAL UPSTREAM | Preserve upstream `cfg.input_mode` and `io_serial()`; when enabled use target UART/DMA/parity/half-duplex configuration instead of STM32 registers | Conditional `hal_serial.c` + shared decoders | rel17 already defines these modes; support selection is source/board-driven |
| APP-I08 | `mcu/*/config.c` / `init()` | REUSE | Existing `void init(void)` | AM13E `config.c` | MCU clock, pinmux, interrupts and hardware protection naturally belong to existing platform hook |
| APP-I09 | `mcu/*/config.c` / `adctrig()` | REUSE | Preserve `adctrig()` and only sample channels enabled by rel17 `SENS_MAP`, temperature and analog-input configuration | `hal_adc.c` | The compiled sensor set derives from the rel17 target/board configuration |
| APP-I10 | `adcdata(int t,int u,int v,int c,int a)` | REUSE | Preserve upstream callback argument meanings and rel17 scaling/limit logic; map physically present target channels without inventing additional measurements | `hal_adc.c` + shared Application data path | No extra product current/telemetry channel is created by this MCU port |
| APP-I11 | `sys_tick_handler()` / `pend_sv_handler()` | ISOLATE | `A10`; native vectors call existing handlers | `hal_system.c` / startup | Rel17 drives 16 kHz timebase and deferred work; STK/SCB setup differs |
| APP-I12 | Main `delay()`, `__WFI()`, `__disable_irq()`/`__enable_irq()` | PRESERVE / NATIVE CMSIS | Use Cortex-M33 CMSIS primitives and rel17 `tick`; no new generic HAL | Shared main + M33 startup | M33 already provides WFI/interrupt-control primitives; unnecessary wrappers increase API count |
| APP-I13 | Main arming wait, TIM6 250-ms countdown | ISOLATE | Use existing 16 kHz `tick` as an AM13E-only elapsed-time implementation | Shared AM13E branch in `main.c` | A dedicated timer-control HAL is unnecessary for this one wait; preserve zero-throttle reset behavior |
| APP-I14 | `src/io.c` / `IWDG_KR` start/feed | ISOLATE | `A11`, `A12` | `hal_system.c` | Watchdog register sequences do not port to M33 |
| APP-I15 | Main reset cause / `RCC_CSR`, `hard_fault_handler()` | ISOLATE | `A13`, `A14` | `hal_system.c` | Native reset flags and fatal behavior must be supplied without fake RCC bits |
| APP-I16 | `src/util.c` / `savecfg()` | ISOLATE | Retain `savecfg()`; `A15(...)` stores within `0x4000..0x4FFF` only using SRAM-safe Flash operations | `hal_storage.c` | ESCape32 parameters must persist across Application reflashing, including when execution overlaps Bank0 |
| APP-I17 | `resetcfg()`, `checkcfg()`, runtime `cfg` startup | PRESERVE + VALIDATE | Read the ESCape32 4 KiB parameter region, check record compatibility, else initialize `cfgdata`; do not touch Reserved | shared source + `hal_storage.c` | Application image update preserves the configuration and Reserved regions; a stale parameter marker does not prove Application validity |
| APP-I18 | `src/util.c` / `initgpio()`, `hallcode()` | TARGET-DEPENDENT | Existing signatures where selected | Optional `config.c` GPIO | Generic STM32 GPIO macro expansion must not compile into AM13E, even if feature is unselected |
| APP-I19 | `initled()` / `ledctl(int)` | REUSE FOR ALL SOURCE OPTIONS | Existing hooks, no new LED HAL | `config.c` or optional GPIO | Preserve the source hook and configure `LED_CNT=0` as specified by existing board options |
| APP-I20 | `hsictl(int)` | CONDITIONAL UPSTREAM | Preserve calibration behavior if `cfg.throt_cal` selected; map source clock correction to native AM13E clock controls and preserve calibration behavior | Conditional clock adapter | STM32 HSI trim step values are not portable |
| APP-I21 | `playmusic()`, `playsound()`, `beep()` | CONDITIONAL UPSTREAM | Preserve upstream motor-sound functions; requires a native bridge-audio actuation path when enabled, isolated from normal six-step commutation | Conditional motor audio adapter | Existing rel17 behavior should not be silently dropped |
| APP-I22 | `src/telem.c`: `inittelem()`, `sendtelemdata()`, ISR bodies | ISOLATE | Keep upstream KISS/iBUS/S.Port/CRSF/MSB/HoTT telemetry under original mode flags; native UART/DMA transport for modes selected by target; inverted DShot reply remains the existing `src/io.c` path | shared `src/telem.c` + conditional native transport | Feature existence and selection come from rel17, not an external telemetry workflow |
| APP-I23 | `src/telem.c` / `sendkiss()`, CRSF and other packet builders | PRESERVE | No new HAL API | Shared protocol source | Pure packet construction is not a TI peripheral |
| APP-I24 | `src/prog.c`: `execcmd()`, `execcrsfcmd()` | PRESERVE | Keep rel17 command/programming behavior; no new service-tool identity, version field or parameter-model protocol is implied | shared `src/prog.c` | This port does not create external service requirements |
| APP-I25 | `src/common.h`, `src/defs.h`, board `config.h` | ISOLATE | Conditional AM13E CMSIS/SDK includes and minimal feature macros | Shared headers + new MCU config | Unconditional libopencm3/STM32 imports block any native AM13E compilation |
| APP-I26 | PWM frame decoder / `servoirq()` | ISOLATE | `C05(high_us, period_us)` performs legacy pulse validation, normalized command/failsafe and watchdog policy | `hal_input.c` calls shared decode | Separate native edge timing from ESCape32 throttle policy |
| APP-I27 | Rel17 inverted DShot / `dshotinv`, `dshotval`, GCR mapping | CONDITIONAL UPSTREAM + ISOLATE | `A16(encoded_gcr20)` as native output primitive only for the upstream inverted mode; preserve prior-response data preparation and TX completion/re-arm | `hal_input.c` | Rel17 mode, not a newly imposed always-on feature |
| APP-I30 | `adcdata()` / sensing + telemetry | BOARD-DEPENDENT | Preserve `adcdata()` monitoring/telemetry variables (`volt`, `curr`, `temp1`/`temp2`) only for configured sensors; do not add a current data stream not modeled by rel17 | Board sensors + `adcdata()` | rel17 compile defaults and physical board wiring define available readings |
| APP-I31 | ESCape32 reboot request / `scb_reset_system()` | ISOLATE | Motor safe-off via `A04`, then native Cortex-M33 software reset; Boot receives consistent entry request/handshake | shared `src/io.c` + `hal_system.c` | Service entry must safely stop the motor before Bootloader reset; not a fatal-error-only path |

**Upstream receiver modes:** rel17 `input_mode=0` contains auto-identification among Servo/Oneshot125/DShot; analog and various UART protocols exist under other mode/target conditions. `servoirq()`/`setthrot()` pulse handling is reused via `C05`; DShot decode via `C04`; inverted DShot physical reply uses `A16` only when the source mode is enabled. Native hardware ownership must follow the actual rel17 ISR/DMA sequence. No external product baseline dictates a simultaneous capability set or new protocol.

**Parameter and sensing rules:** Application `A15` writes only `0x4000..0x4FFF` and retains `savecfg()`/`resetcfg()` semantics. `0x5000..0x5FFF` remains Reserved and untouched by normal operations. `adcdata()` remains the upstream callback and uses only the rel17 target's configured analog input/sensor channels. No new telemetry measurement is specified here.

## 3. ESCape32-derived Bootloader — protocol and operations

| ID | Existing source / function | Action | Interface / separation | Owner | Reason |
|---|---|---|---|---|---|
| BOOT-M01 | `boot/src/main.c` / command dispatch, `CMD_PROBE=0` | PRESERVE | Original `recvval()` and `sendval(RES_OK)` unchanged | Shared `boot/src/main.c` | Original probe command and reply must remain |
| BOOT-M02 | `CMD_INFO=1` / `DBGMCU_IDCODE` | PRESERVE + ISOLATE | `B03` obtains authentic AM13E ID; retain 32-byte `senddata()` shape, revision/IO_PIN placement | `boot/mcu/AM13E/hal_start.c` | Original information command must remain; identity is MCU-specific |
| BOOT-M03 | `CMD_READ=2` | PRESERVE + FLASH LAYOUT | `B01(uint16_t effective_block,...)`, original 8-bit block/count and CRC32-framed response, `CMD_WINDOW=6` high window | `boot/mcu/AM13E/hal_flash.c` | Preserve read semantics and wire format; 488 KiB APP exceeds 8-bit block range |
| BOOT-M04 | `CMD_WRITE=3` | PRESERVE + FLASH LAYOUT | `B02(uint16_t effective_block,...)`, original 1 KiB CRC block and result value; APP-only 2 KiB sector RMW, Bank0 SRAM command path | `boot/mcu/AM13E/hal_flash.c` | Preserve write/ACK behavior; adapt region and erase/program geometry |
| BOOT-M05 | `CMD_UPDATE=4` / `update()` | **PRESERVE FULL OPERATION** | Keep original receive loop, per-block `RES_OK`, short final block, `update(char*,const char*,int)` and success-reset/error-return sequence; 16 KiB RAM-staged Boot self-update | `boot/src/main.c` + `boot/mcu/AM13E/hal_flash.c` | Bootloader self-update is an original rel17 feature; MCU-specific Flash must not delete it |
| BOOT-M06 | `CMD_SETWRP=5` / `setwrp()` | **PRESERVE FULL OPERATION** | Preserve option `0x33` Off / `0x44` Boot / `0x55` Full and `setwrp(int)` reset-on-success/error-on-return model; map to reversible AM13E static write protection | `boot/src/main.c` + `boot/mcu/AM13E/hal_protection.c` | Original write-protection feature cannot be reduced to an unsupported stub or transient dynamic mask |
| BOOT-M07 | Default dispatch / marker and vector jump | PRESERVE + FLASH LAYOUT | Keep `0x32EA` check at relocated `CFG_BASE`, launch vectors at relocated `APP_BASE=0x6000`; `B04`/`B05` check Cortex-M33 entry and transfer | `boot/src/main.c` + `boot/mcu/AM13E/hal_start.c` | Original marker and jump semantics remain; old `_rom_end+PAGE_SIZE` address assumption changes |
| BOOT-M08 | Boot reboot ACK / `RCC_CSR` | PRESERVE + ISOLATE | `B06` native reset cause; preserve ACK after recognized reboot | `boot/mcu/AM13E/config.c` | Native reset flags replace STM32-specific bits |
| BOOT-M09 | New `CMD_WINDOW=6` | ADD FLASH-LAYOUT EXTENSION | Complement-coded `0/1`; `effective_block=window*256+legacy_block`; 0..487 valid, window 0 at entry; unchanged READ/WRITE framing; host first identifies AM13E via original `CMD_INFO` | `boot/src/main.c`, B01/B02 | Minimum additive addressability extension; no original ID altered |
| BOOT-M10 | Original image-valid marker / `Cfg.id` | PRESERVE + RELOCATE | Keep upstream `0x32EA` in Application configuration and direct jump eligibility; no mandatory new manifest/commit or cryptographic image protocol | `boot/src/main.c` + config linker | Rel17 marker/jump are original behavior; Flash layout changes their address relationship |

## 4. ESCape32-derived Bootloader — transport and startup

| ID | Existing source / function | Action | Interface / separation | Owner | Reason |
|---|---|---|---|---|---|
| BOOT-I01 | `boot/src/io.c` / `initio()` | REUSE | Existing `void initio(void)` | `boot/mcu/AM13E/hal_uart.c` | UART/timeout/init is physical transport, not wire protocol |
| BOOT-I02 | `boot/src/io.c` / `recvbuf()`, `sendbuf()` | REUSE | Existing `int recvbuf(char*,int)` / `void sendbuf(const char*,int)` | `hal_uart.c` | Maintain byte transport contract; remove legacy TIM/USART registers |
| BOOT-I03 | `recvval()`, `sendval()`, `recvdata()`, `senddata()` | PRESERVE | Existing rel17 framing | Shared `boot/src/io.c` | Complement-coded values and CRC32 block framing remain at protocol layer |
| BOOT-I04 | `boot/src/util.c` / `crc32()` | REUSE / REPLACE IMPLEMENTATION | Existing `uint32_t crc32(const char*,int)` | Boot platform software CRC or valid TI CRC unit | Wire result must match rel17; do not copy STM32 CRC registers |
| BOOT-I05 | `boot/src/util.c` / `write()`, `update()`, `setwrp()` | REUSE ORIGINAL ENTRY POINTS | Keep upstream signatures and call sites; provide AM13E-only implementations in `hal_flash.c` / `hal_protection.c` behind MCU build guards | `boot/mcu/AM13E/` | Full preservation of original write, self-update and WRP operations; no duplicate public HAL APIs |
| BOOT-I06 | `boot/mcu/STM32*/config.c` / `init()` | REUSE | Existing `void init(void)` | `boot/mcu/AM13E/config.c` | Boot target hardware initialization remains under original MCU structure |
| BOOT-I07 | `boot/src/common.h` / STM32 includes and linker symbols | ISOLATE | AM13E-target header/partition symbols | Shared header + new linker map | Do not mix libopencm3 and TI CMSIS register structures |
| BOOT-I08 | `boot/src/main.c` / `void main(void)` | ISOLATE | AM13E `int main(void)` entry bridge (build integration, not a HAL API) | `boot/mcu/AM13E/entry.c` | TI GCC startup owns Reset/vector and calls an `int main(void)` ABI |

**Boot source compatibility:** all six original commands are **functionally preserved**, not just recognized by ID. `CMD_UPDATE=4` keeps RAM-staged Boot self-update and reset-on-success, and `CMD_SETWRP=5` keeps Off/Boot/Full protection and reload/reset behavior. The added `CMD_WINDOW=6` extends READ/WRITE addressing without replacing their wire formats; updated Host address selection is needed for the upper APP. Keep rel17 unsigned-complement byte framing and CRC32 results. Old hosts that expect a Config-prefix image or fixed 256 KiB address space are **not** automatically compatible. Normal Boot updates must not write the configuration or Reserved regions.

**Bank conflict:** Boot sits in Bank0 while APP `0x6000..0x3FFFF` also lies in Bank0. Same-bank programming requires SRAM-resident Flash command execution. A cross-bank-only approach from the former Bank1 APP mapping does not apply. The original `Cfg.id=0x32EA` marker at `CFG_BASE`, combined with the relocated APP vector eligibility check, remains the source-equivalent Boot decision; image completeness after interruption is a documented upstream limitation, not justification for an invented mandatory manifest.

## 5. Build, Linker, configuration, partition ownership

| ID | Source / module | Action | Contract / mapping | Responsibility |
|---|---|---|---|---|
| SYS-01 | Root `CMakeLists.txt` / `add_target()` | ISOLATE | AM13E target-specific compile/link/select branch, no new HAL | Preserve legacy build workflow; avoid STM32 size/link/flash defaults |
| SYS-02 | `boot/CMakeLists.txt` | ADD | Native AM13E boot target using existing `add_target()` | Keep rel17 dual target organization |
| SYS-03 | `mcu/AM13E/config.cmake`, `config.h`, `config.ld` | ADD | Board capabilities, M33 toolchain/runtime, symbolic partition | Separate MCU capabilities from actual ESC pin assignments |
| SYS-04 | `boot/mcu/AM13E/config.cmake`, `config.h`, `config.ld` | ADD | Same Boot/App partition constants; Boot self-update RAM staging + Flash-RAM code; C-M33 vectors; protection configuration | New Boot MCU target | Avoid address disagreement, duplicate vectors, missing Boot self-update support |
| SYS-05 | App/Boot `entry.c` | ADD | Wrapper from TI `int main` ABI to AM13E-gated rel17 entry | TI startup and ESCape32 source use different `main` declarations |
| SYS-06 | Selected TI SDK DriverLib source files | ADD BUILD DEPENDENCY | Only AM13E target, no whole SDK firmware solution | Avoid including a second main, Reset/vector or linker script |
| SYS-07 | Original `mcu/{STM32*,AT32*,GD32*}/`, `boot/mcu/STM32*/` | PRESERVE | No new dependency | New MCU support must not force legacy platform refactoring |
| SYS-08 | Persistent ESCape32 configuration and reserved Flash | ISOLATE | `0x4000..0x4FFF` ESCape32 configuration; `0x5000..0x5FFF` Reserved; `A15` write authorization limited to configuration | Preserve both intervals during APP reflashing; no Boot command access to configuration or Reserved |
| SYS-10 | Rel17 source features / target board contract | ADD BOARD CONFIG | Select upstream `INPUT_MODE`, `COMP_MAP`, `SENS_MAP`, Hall/LED/serial/other feature macros only as supported by actual AM13E board; gate pin truth table and analog routings explicit | No external feature checklist or invented pin assignment; preserve upstream compile semantics |

**Source-derived configuration:** select from rel17's existing `INPUT_MODE`, `COMP_MAP`, `SENS_MAP`, `HALL_MAP`, `PWM_ENABLE`, `BRUSHED`, `SINE_RANGE`, LEDs, serial and telemetry compile/runtime branches. `PWM_ENABLE` controls motor-output behavior and must not be conflated with PWM receiver decoding. Omitting `SENS_MAP` is a rel17-defined no-`SENS_MAP` default, not permission to invent sensor channels. All source-present conditional modes remain in scope as guarded capabilities; none is artificially made mandatory or deliberately excluded by another product profile. When a board lacks a required signal for a selected upstream branch, the AM13E target configuration must explicitly constrain that branch.

**Static interface contract vs architecture:** the 16 KiB Bootloader, 4 KiB ESCape32 configuration, 4 KiB Reserved and 488 KiB APP are an architecture *plan*. Firmware-size/linker bounds, 2 KiB sector behavior and the fixed Application `APP_BASE` relocation remain source/SDK integration constraints, not claimed demonstrated results.

## 6. Mapping to minimal new interfaces

| Private IDs | Sources / reason |
|---|---|
| `A01`–`A06` | Application bridge drive, PWM stage, commutation update and safe stop (`APP-M02`, `APP-M03`, `APP-M08`–`APP-M12`) |
| `A07`–`A09` | BEMF phase sampling/capture deadline and timeout (`APP-M04`–`APP-M08`) |
| `A10`–`A14` | Application tick, watchdog, fault/reset (`APP-I11`–`APP-I15`, `APP-M12`, `APP-I31`) |
| `A15` | Application Flash configuration transaction (`APP-I16`, `APP-I17`, `SYS-08`) |
| `A16` | Existing rel17 inverted-DShot return waveform / timing (`APP-I03`, `APP-I27`), only when source mode enabled |
| `C01`–`C03` | Native commutation and BEMF event callbacks (`APP-M06`–`APP-M08`) |
| `C04` | DShot capture, CRC/command processing and polarity (`APP-I02`–`APP-I04`) |
| `C05` | Existing rel17 Servo/Oneshot pulse timing (`APP-I06`, `APP-I26`) |
| `B01`–`B05` | Common Boot read/write, AM13E identity, image eligibility and direct jump (`BOOT-M02`–`BOOT-M04`, `BOOT-M07`) |
| `B06` | Reset-cause signature shared with Application `A13`, separately linked (`BOOT-M08`) |

The interface ID references are normative links to [Interface_Contracts.md](Interface_Contracts.md). Other functionality (protocol parsing, CRC and existing `init()`/`compctl()`/`adctrig()`/`initio()` hooks) does not justify new ESCape32 motor HAL functions. The 26 private symbols are the current common MCU seam, **not a feature ceiling**. Existing rel17 `write()`/`update()`/`setwrp()` are separate source-defined functions implemented on AM13E, not counted as newly invented APIs. Each conditional upstream feature must retain a concrete target hook/adapter contract; do not mark it unsupported simply to keep API counts small. API count must follow source needs, not be frozen for its own sake.

## 7. Version adaptation

When ESCape32 or TI SDK changes, compare **function/module identity, behavior, input/output contract and the partition ABI**, not source line numbers. Update the affected rows only; do not maintain a separate Evidence inventory or Source Change Plan. 

## 8. Original Boot command preservation checklist

| Original command | Host request/reply contract | Native implementation hook | Status in design |
|---|---|---|---|
| `CMD_PROBE=0` | `recvval()` → `RES_OK` | Existing transport | **PRESERVE** |
| `CMD_INFO=1` | 32-byte CRC32-framed reply | `B03`, native `IO_PIN` | **PRESERVE** |
| `CMD_READ=2` | 8-bit block + count → framed data | `B01` | **PRESERVE** (APP address relocation only) |
| `CMD_WRITE=3` | 8-bit block + CRC data → `RES_OK/RES_ERROR` | `B02`, existing `write()` entry | **PRESERVE** (APP address relocation only) |
| `CMD_UPDATE=4` | 1 KiB Boot chunks; interim ACK; reset on success | Existing `update()` AM13E implementation | **PRESERVE FULL OPERATION** |
| `CMD_SETWRP=5` | `0x33/0x44/0x55`; protection reload/reset | Existing `setwrp()` AM13E implementation | **PRESERVE FULL OPERATION** |
| `CMD_WINDOW=6` | One complement-coded `0/1`; one status reply | `B01` / `B02` high-window state | **ADD ONLY FOR FLASH LAYOUT** |

`CMD_UPDATE` / `CMD_SETWRP` are not optional and shall not be mapped to a generic unsupported response. AM13E static protection implementation must preserve reversible Off/Boot/Full semantics under the selected device security configuration; where this is blocked by irreversible NONMAIN configuration, resolve the target configuration rather than change the upstream feature set. See [Interface_Contracts.md](Interface_Contracts.md).
