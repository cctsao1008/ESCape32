# ESCape32 rel17 → TI AM13E23019 — Integration Design

**Revision:** v1.6  
**Status:** Source-derived, full-feature Application and Bootloader HAL porting design; **design only**  
**Primary baseline:** ESCape32 rel17, archive commit `1d718c143380c3eb7e6581478990e1496632e5b8`  
**MCU reference:** supplied `am13e2x_sdk-main.zip` snapshot; target AM13E23019  
**Companion specifications:** [Integration_Mapping.md](Integration_Mapping.md) · [Interface_Contracts.md](Interface_Contracts.md)
**v1.6 scope:** File-organization revision only: nested Application and Boot HAL directories and AM13E-scoped CMake source selection. Existing rel17 feature, Flash-layout, Boot command, mapping ID and private interface semantics are unchanged.

## 1. Authority and invariants

This work adds an **AM13E** native target to upstream ESCape32 rel17. The shipped Application is the original ESCape32 Core, not a compatible rewrite. The shipped Bootloader is derived from ESCape32's existing `boot/src/`, not a replacement updater.

1. **Preserve all source-present behavior and build-time options**: algorithms, runtime modes, commands, telemetry, startup/sine/hybrid/Hall/brushed, braking, parking, audio, auxiliary I/O, safety, configuration and optional features. An upstream option may require a matching board signal or mutually exclusive configuration, as it already does on legacy targets; do not delete it from the port design.
2. **No new application behavior or protocol** is introduced by the HAL. Private calls translate MCU hardware differences only. Existing source-level Hooks take precedence over new calls.
3. **The selected Flash layout is the sole planned architecture exception**. Original Boot commands `0`–`5` and their semantics remain; any command introduced solely to express the larger/rearranged Flash address range is additive and version-detected.
4. **The existing `add_target()` build pattern survives**. STM32, AT32 and GD32 trees and outputs are not restructured for this port.
5. This is a **design specification, not compiled AM13E firmware**. Unknown Board pin/driver/protection realization must be bound by the actual target configuration, not invented here. No Mock execution, firmware porting or physical testing is a design acceptance condition.

### Native repository layout (planned additions; aligned to upstream MCU directories)

The **Application naming baseline** is `mcu/STM32G431/` in rel17; the **Boot naming baseline** is `boot/mcu/STM32G4/`. AM13E adds its own native target directories. Their top-level filenames match the existing ESCape32 MCU pattern, with a single private `hal/` subdirectory **inside each AM13E MCU target** for DriverLib and register-level details. These are planned paths, not modified source files.

```text
ESCape32/                              # Repository root (e.g. am13e-port-v2/)
├── CMakeLists.txt                     # Existing add_target(name mcu ...) function
├── src/                               # Original Application Core (preserved)
├── mcu/
│   ├── STM32G431/                     # Existing reference, unchanged
│   │   ├── config.c
│   │   ├── config.h
│   │   ├── config.cmake
│   │   ├── config.ld
│   │   └── preset.c
│   ├── STM32*/ AT32*/ GD32*/          # Other existing targets, unchanged
│   └── AM13E/                         # New Application MCU target
│       ├── config.c                   # Original ESCape32 MCU Hooks / HAL dispatch
│       ├── config.h                   # MCU configuration and Hook declarations
│       ├── config.cmake               # AM13E compiler / TI SDK / source selection
│       ├── config.ld                  # Application memory layout and linker symbols
│       ├── preset.c                   # Original sinedata[] contract
│       └── hal/                       # AM13E Application-only hardware implementation
│           ├── hal_motor.c            # Illustrative module, not a fixed file inventory
│           ├── hal_bemf.c
│           ├── hal_input.c
│           └── ...                    # Remaining units driven by Mapping
└── boot/
    ├── CMakeLists.txt                 # Original Boot add_target(...) declarations
    ├── src/                           # Original Boot commands and protocol (preserved)
    └── mcu/
        ├── STM32G4/                   # Existing Boot naming reference
        │   ├── config.c
        │   ├── config.h
        │   ├── config.cmake
        │   └── config.ld
        ├── STM32F0/ and STM32G0/      # Original Boot targets, unchanged
        └── AM13E/                     # New Boot MCU target
            ├── config.c              # Original Boot MCU Hooks / HAL dispatch
            ├── config.h              # Boot MCU and transport definitions
            ├── config.cmake           # Independent Boot SDK and source selection
            ├── config.ld             # Boot program/RAM memory map
            └── hal/                  # AM13E Boot-only hardware implementation
                ├── hal_flash.c        # Illustrative, not a fixed module inventory
                ├── hal_uart.c
                ├── hal_protection.c
                └── ...
```

**Required native-name counterparts:** five Application files (`config.c`, `config.h`, `config.cmake`, `config.ld`, `preset.c`) and four Boot files (`config.c`, `config.h`, `config.cmake`, `config.ld`). `preset.c` retains the upstream `const uint16_t sinedata[]` contract; the upstream Boot reference has no `preset.c`. These files are entry/configuration seams, **not copied STM32 peripheral implementations**.

| File/location | Responsibility and boundary |
|---|---|
| `mcu/AM13E/config.c` | Implement/reuse ESCape32 source-visible Hooks such as `init()`, `compctl()`, `adctrig()`, `io_serial()` and `io_analog()`; delegate AM13E register/DriverLib operations to its own `hal/`. |
| `mcu/AM13E/config.h` | MCU and board-specific feature/role definitions and Hook declarations; never emulate STM32 registers. |
| `mcu/AM13E/config.cmake` | AM13E toolchain/CMSIS/DriverLib configuration and Application HAL source selection, under the existing build entry. |
| `mcu/AM13E/config.ld` | Original ESCape32 linker-symbol contract mapped to AM13E Flash/SRAM, with variable Application image size up to the configured maximum. |
| `mcu/AM13E/preset.c` | Preserve upstream `sinedata[]` type/symbol/table behavior. |
| `mcu/AM13E/hal/` | Application-only motor, BEMF, ADC, capture/DMA, auxiliary and runtime/system hardware implementation; internal headers allowed if actually needed. |
| `boot/mcu/AM13E/config.c` and `config.h` | Original Boot target entry Hooks, definitions and dispatch; no motor-control dependency. |
| `boot/mcu/AM13E/config.cmake` and `config.ld` | Independent Boot target build and memory ownership. |
| `boot/mcu/AM13E/hal/` | Boot-only Flash/read/write/protection, communication and image entry hardware implementations; no Application HAL linkage. |

#### Source-discovery and build integration (one existing function, two calling directories)

In rel17, root `CMakeLists.txt` defines `add_target(name mcu ...)` and its original `file(GLOB srcs src/*.c mcu/${mcu}/*.c)` selects `.c` files **only at the MCU directory root**. The same function is invoked by the root Application targets and by `boot/CMakeLists.txt` Boot targets. It does **not** automatically discover either new `hal/` directory.

Preserve the `add_target()` interface and existing top-level GLOB. Add this **AM13E-only conceptual extension** in that function *after* the `add_executable(${elf} ${srcs})` line, so the same rule applies when invoked for either image:

```cmake
# Proposed change to existing add_target(); illustration, not implemented.
if(mcu STREQUAL "AM13E")
    file(GLOB am13e_hal_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/${mcu}/hal/*.c")
    target_sources(${elf} PRIVATE ${am13e_hal_sources})
    target_include_directories(${elf} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/${mcu}/hal")
endif()
```

`${CMAKE_CURRENT_SOURCE_DIR}` is resolved for the invoking source directory: repository root for Application and `boot/` for Boot. This keeps the legacy target discovery unchanged and ensures the Boot target never compiles Application HAL files. The two AM13E `config.cmake` files still independently select TI SDK/CMSIS/DriverLib dependencies, startup/vector inputs, compile/link flags and any extra non-HAL source files. The proposed nested `hal/` locations require this explicit CMake selection; **simply creating the directories is insufficient**.

The root build also assumes 32/64 KiB `FLASH_SIZE`, a libopencm3 environment and `st-flash` programming. Their AM13E-specific replacements, one valid startup/vector per image, Cortex-M33 ABI, linker integration and programming tool are separate necessary **AM13E-only** adaptations; do not affect STM32/AT32/GD32 builds. Boot and Application shall each produce an independent ELF/image. `AM13E` is the build-directory token; AM13E23019 is the selected device.

**Isolation rule:** `mcu/AM13E/hal/` and `boot/mcu/AM13E/hal/` are physically and link-wise separate. Do not put ESCape32 algorithms, input decoders, Boot command parsers or product policy into either HAL directory. Similar low-level operations may share reviewed SDK code as **separately compiled source/library dependencies** if actually justified, never by linking one image against the other image's HAL objects. All listed `hal_*.c` filenames are proposed internal responsibilities, not a new public HAL framework or a mandatory file count.

## 2. Application architecture: retain Core, isolate actual hardware seams

| Upstream owner | Must remain in ESCape32 | AM13E hardware ownership |
|---|---|---|
| `src/main.c` | `main()`, `nextstep()`, `laststep()`, `iftim_isr()`, `tim1_com_isr()`, `tim3_isr()`, protection/startup/arming, 360-step sine and brushed policies | MCPWM waveform and synchronized update, BEMF route/capture/deadline, Hall edge timing, GPIO, hardware safety, system timer/ISR aliases |
| `src/io.c` | `entryirq()`, `calibirq()`, `servoirq()`, `dshotirq()`, `iotim_dma_isr()`, `dshotcrc()`, all mode-specific decoders and command policy | PWM/Oneshot capture, DShot RX/TX DMA/timing, input-mode transitions, UART/parity/half-duplex, software UART, input timeout and AUX capture |
| `src/telem.c` | KISS/iBUS/S.Port/CRSF/MSB/HoTT packets, timer-driven telemetry cadence, extended DShot data | Telemetry UART/DMA half-duplex RX/TX, idle/timeout/TC events and queued transfer ownership |
| `src/prog.c` | CLI/programming commands and CRSF command parsing | No new command processor; use transport hooks supplied by other modules |
| `src/util.c` | config validation/defaults, PID, CRC, `playmusic()`/`playsound()` parsing, `resetcom()` logical intent | Native GPIO/Hall/LED/BEC/ERPM/PARK operations, timer/bridge audio waveform, Flash persistence, clock-trim implementation |
| `mcu/AM13E/` | Existing `init()`, `compctl()`, `adctrig()`, `io_serial()`, `io_analog()`, as applicable `hallcode()`, `initled()`, `ledctl()`, `hsictl()` | AM13E DriverLib/CMSIS peripheral selection and physical resource implementation |

**Critical correction from v1.4:** a Source/Sink/Floating six-step mask alone is *not* the ESCape32 output abstraction. `nextstep()` includes **three independently modulated sine channels**; the main loop includes a **brushed bridge pattern**; `resetcom()`/`laststep()`/sound functions emit different forced and PWM states; compile choices `PWM_ENABLE`, `INVERTED_HIGH`, `FULL_DUTY`, `SW_BLANKING` affect hardware events and gate semantics. The common private drive-plan contract therefore covers the **full rel17 output-state set**, not merely sensorless six-step. The shared source still computes the states; AM13E only realizes them.

### Event fidelity is mandatory

- `nextstep()` calls `compctl(pcc)` and updates `pcc=cc`, retaining the **previous-phase comparator-selection** behavior; zero-cross selection must not be advanced one commutation.
- `iftim_isr()` first tests enabled timeout/overflow (`UIF`) and exits on timeout. Only then does it evaluate input capture, reject early crossings (`t < ival/2`), calculate `fast`, `ival`, delay and **reset the schedule timer in the accepted ISR**. A backend must not substitute the physical capture timestamp as the timer-reset epoch.
- `tim1_com_isr()` performs target-dependent output state cleaning before `nextstep()` on some legacy platforms; the AM13E drive-plan/commit contract must express any equivalent state transition coherently.
- Hall input `tim3_isr()` has **capture and timeout** variants plus phase synchronization; it is not BEMF comparator input repackaged as Hall.
- `src/io.c` has a **stateful input-mode discriminator** and source-specific half-duplex, DMA, soft-UART and inverted-DShot timing. These are shared Application behaviors; native ISR adapters only deliver/realize the requisite events.
- Sensor data passed to `adcdata(int t,int u,int v,int c,int a)` retain rel17 meanings, channel order and scaling. No extra current, telemetry or safety feature is introduced from external architecture documents.

## 3. Feature/variant policy: full upstream coverage, not all features forced on one board

Every feature and compile branch identified below has an explicit source → native-port disposition in [Integration_Mapping.md](Integration_Mapping.md). The *upstream* board-selection model applies:

| Existing family | Source-driven control | Native contract dependency |
|---|---|---|
| Motor | 6-step, sine startup/handoff, brushed, damping, drag/active braking, stop/coast/lock, PWM output variants | Multi-state three-phase drive plan, buffered duty/phase update, blanking and hardware trip |
| Feedback | BEMF zero crossing, Hall/hybrid and Hall timeout; sensor/ADC monitoring | Comparator routing, scheduler, optional Hall capture and GPIO, ADC callbacks |
| Receiver | PWM/Oneshot, DShot normal/inverted, serial, iBUS, SBUS/SBUS2, CRSF, EXBUS, HoTT, analog and `IO_AUX` | Capture/DMA, serial modes, RX/TX rearm and analog input hook |
| Telemetry | KISS, iBUS, S.Port, CRSF, MSB, HoTT, extended DShot | Native telemetry transport with half-duplex/queue behavior |
| Auxiliary | `LED_MAP`, `LED_WS2812`, `BEC_MAP`, `ERPM_PIN`, `PARK_PIN`, `IO_RXTX`, `IO_AUX`, audio | GPIO/LED hooks and private role-based I/O; bridge audio and auxiliary timing |
| Runtime/service | CLI, programming, PID, CRC, parameter store/reset, watchdog/reset, startup melody | Keep upstream parsers; native memory/clock/watchdog/Flash operations |
| Build variants | `PWM_ENABLE`, `INVERTED_HIGH`, `FULL_DUTY`, `SW_BLANKING`, `USE_XOR`, `USE_COMP2`, `USE_OPAMP`, special board macros | Target configuration must map original *behavior*, not substitute STM32 registers or silently disable branch |

Board-dependent prerequisites (gate-driver electrical truth table, CMPSS analog input multiplexing, capture/serial pin sharing, peripheral resource ownership) are **inputs to selecting an AM13E board target**, not permission to remove features from the ESCape32 source/design. Some source options are mutually exclusive by upstream rules; full-feature preservation means each reachable source behavior has a design path, not simultaneous activation of conflicting pin choices.

## 4. Flash layout: the only accepted planned functional address exception

Flash addresses are MCU-specific **design allocation**, not rel17 upstream constants. AM13E23019 MAIN Flash: two 256 KiB banks, 512 KiB total. The 488 KiB Application space is a **maximum capacity, not a fixed binary image size**.

| Region | Half-open bounds | Size | Policy |
|---|---|---:|---|
| Boot | `[0x00000000, 0x00004000)` | 16 KiB | Retained normal Boot and self-update entry, not overwritten by ordinary Application programming |
| ESCape32 Config | `[0x00004000, 0x00005000)` | 4 KiB | Preserves upstream Cfg parameters, including `Cfg.id == 0x32EA` marker behavior |
| Reserved | `[0x00005000, 0x00006000)` | 4 KiB | No normal write/read mapping into another function; preserved |
| Application | `[0x00006000, 0x00080000)` | **maximum 488 KiB** | Fixed `APP_BASE`; actual image length is linker/host-provided and bounded by region |

The Application begins in **Bank0** and continues in Bank1. Consequently, both Boot programming parts of Application and App saving Config may touch the executing bank. Flash command-execution routines must be placed/executed from SRAM as required by the AM13E TRM/SDK; do not assume cross-bank RWW is sufficient for every operation. For the supplied SDK, `DL_FLASH_SECTOR_SIZE=0x800` and `DL_Flash_program()` implementation applies **16-byte address alignment** even though its Header says 8-byte: follow the conservative code path and make the discrepancy a device-driver integration note. Resource/protection granularity differs from sector erase granularity.

### Boot-to-Application handoff

Keep the rel17 gate based on `Cfg.id == 0x32EA` at the relocated Config region and jump to vector/Reset at fixed `APP_BASE`; Cortex-M33 stack, vector and interrupt handling is AM13E-specific. The original marker **does not prove that a newly transferred image is complete**; do not attribute a new signed manifest/A-B rollback feature to rel17. The other 4 KiB region remains Reserved, not another parameter store.

### Original programming-stream contract under a larger/rearranged layout

The upstream `CMD_READ/WRITE` stream begins at `_rom_end` (the *Config prefix*, not the App vector) and uses 1 KiB logical blocks. v1.4 incorrectly redirected **all original blocks** to Application and thereby lost the original Config-prefix read/write capability.

**v1.6 selected layout adapter:** retain **logical Config-prefix semantics** while skipping physical Reserved Flash:

- Logical blocks **0–3** map to physical Config `[0x4000,0x5000)`.
- Logical blocks **4–491** map to physical Application `[0x6000,0x80000)`; physical Reserved `[0x5000,0x6000)` is not part of the programming stream.
- The original 8-bit per-command block field is unchanged. New `CMD_WINDOW=6` selects high logical window `0` or `1`; `effective_block = 256 × window + received_block`. Only **0–491** are valid. Logical read/write packets remain within one 1 KiB block, and both windows use original `CMD_READ/WRITE` framing and result rules.
- **A Config + Application host image stream must be rebuilt for this layout**: its Config prefix now occupies 4 KiB instead of the 2 KiB STM32G4 baseline, and the Reserved gap is omitted. A legacy Host binary cannot be assumed compatible merely because the command IDs survive.
- Boot `CMD_READ` returns the bytes of the original logical stream, including Config blocks 0–3. `CMD_WRITE` can write Config blocks 0–3 and App blocks 4–491 when allowed by the selected protection mode. Neither operation may touch Reserved or exceed bounds. Boot updater must preserve partial 2 KiB physical sectors using read-modify-erase-program as necessary.

This translation is the **minimum explicit Flash-layout-related behavioral adaptation** required to keep the source's Config-first programming model. No general arbitrary-address read/write interface is introduced. See [Interface_Contracts.md](Interface_Contracts.md) for parser/reply and protection cases.

### Six original Boot commands: preserved functions

| Original ID | rel17 behavior that must remain | AM13E substitution |
|---|---|---|
| `CMD_PROBE=0` | Complement-coded ACK | Reuse `recvval()`/`sendval()` |
| `CMD_INFO=1` | 32-byte CRC32-framed info, revision and MCU/IO identity | Authentic AM13E identity and target IO mode; original packet shape |
| `CMD_READ=2` | 8-bit block plus word-count read, CRC response | Bounded Config-first logical translator + optional selected high window |
| `CMD_WRITE=3` | CRC32-framed up-to-1 KiB payload, `RES_OK`/`RES_ERROR` | Logical translator + protected 2 KiB sector RMW, verify-before-success |
| `CMD_UPDATE=4` | SRAM-staged Boot self-update in 1 KiB packets, per-packet ACK; reset on successful `update()` | AM13E SRAM staging, RAM-resident Flash command path; Boot region only |
| `CMD_SETWRP=5` | Subcommands `0x33` Off / `0x44` Boot / `0x55` Full, with reload/reset on success | Native reversible persistent write-protection policy, with exact granularity enforced and no irreversible security lock |

**Only new command:** `CMD_WINDOW=6` for 492 KiB logical Config+Application stream. Only a Host that identified the AM13E extended protocol through existing `CMD_INFO` may send ID 6; upstream bootloaders treat an unknown command as an attempt to leave programming mode. Never claim that old hosts can upload the whole region without extension.

**Protection semantics are a hard compatibility requirement:** `setwrp(0)` must restore write capability; `setwrp(1)` protects Boot while permitting original programming/config operations; `setwrp(2)` protects all MAIN Flash. AM13E sector erase, RAM execution and protection configuration procedures live in Boot `mcu/AM13E/`. An irreversible NONMAIN/security state is **not** an acceptable silent substitute for reversible upstream `setwrp()`. Exact AM13E supported nonvolatile protection sequence remains a silicon/SDK compatibility dependency; the design does **not** assert it has been demonstrated or that any unsupported mode can return fake success.

## 5. CMake, startup and linker integration

Keep root and Boot `add_target(name mcu ...)` conventions. Add only AM13E-selective settings for Cortex-M33 target flags, TI DriverLib/CMSIS includes, MCU-specific Startup/Vector definition, SRAM-executable Flash functions, Flash map, `objcopy` outputs and a suitable programming command instead of `st-flash`. Each image owns precisely one startup and one effective linker script; TI example firmware `main()` is excluded. A target-limited entry wrapper reconciles ESCape32's `void main(void)` with the selected TI Startup expectation without touching legacy targets.

- Preserve the **STM32G431-equivalent Application filename set** under `mcu/AM13E/`: `config.c`, `config.h`, `config.cmake`, `config.ld`, **`preset.c`**. Preserve the **STM32G4-equivalent Boot filename set** under `boot/mcu/AM13E/`: `config.c`, `config.h`, `config.cmake`, `config.ld` (no Boot `preset.c`).
- Existing ESCape32 Hooks remain anchored through each image's `config.c`. Add AM13E-only internal `hal/*.c` modules as needed **under the respective Application or Boot MCU directory**. The original root-level MCU GLOB does not recurse: the AM13E-specific source-selection rule shown in §1 is necessary in the existing `add_target()` function. No vendor register logic or new framework belongs in `src/` or `boot/src/`.
- Keep same symbolic `CFG_BASE`, `APP_BASE`, `BOOT_END`, etc. across both builds; assert `Boot <= 16 KiB`, `Config <= 4 KiB`, and actual application binary `<= 488 KiB` at link/package boundaries.
- The boot service transport is independent of motor control. Preserve original UART/soft-serial, half-duplex and timeout semantics for the selected original Boot target I/O mode; support those original Boot modes rather than arbitrarily hard-coding one new host transport.
- Leave all original MCU target definitions and source files unmodified except for **guarded shared-source hardware seam changes** strictly needed to compile/use AM13E.

## 6. Scope and document discipline

This is a **complete source-feature integration design plan** subject to board-capability binding and target-specific DriverLib feasibility, not a claim of executable firmware equivalence. The companion Mapping identifies **where**, **how** and **why** each source hardware dependency is adapted; the Contracts define exactly what each proposed private function and retained source Hook must do. Neither replaces original ESCape32 algorithm code with an AM13E-specific control rewrite.

No external product features, additional control firmware, design-history CSV, audit evidence directory, protocol framework or new manifest is included. MCU/SDK changes are tracked by stable function/module identifiers and behavior, not source line numbers. Original licensing/release obligations are unaffected.
