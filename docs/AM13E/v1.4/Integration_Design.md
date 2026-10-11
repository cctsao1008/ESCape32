# ESCape32 rel17 to TI AM13E23019 — Integration Design

**Revision:** 1.4 — full-feature native MCU port; additive Boot command extension; design only  
**Firmware source baseline:** ESCape32 rel17 (`1d718c143380c3eb7e6581478990e1496632e5b8`)  
**MCU SDK reference:** supplied `am13e2x_sdk-main.zip` snapshot (`07b1e5b401ed26d0bed59de666ecf2ab52ac1a42`)  
**Platform partition reference:** the previously selected AM13E Flash partition remains a port-integration planning constraint. It is **not** an authority for ESCape32 application features, protocol priorities, input modes, telemetry requirements or optional-feature exclusions.  
**Companions:** [Integration_Mapping.md](Integration_Mapping.md) and [Interface_Contracts.md](Interface_Contracts.md).

## 1. Purpose and design authority

Extend **upstream ESCape32**, rather than placing it inside a new firmware framework. The detailed source-to-platform design covers **the ESCape32 rel17 sensorless six-step Application** and its **ESCape32-derived Bootloader**. The target remains a native ESCape32 MCU extension rather than a separate firmware framework.

ESCape32 rel17 source code, its `README.md`, `src/defs.h`, target CMake definitions and MCU hooks are the **only feature/function authority** for this port. The selected Flash partition is a target-integration choice, not a rel17 feature. AM13E peripheral/Flash capabilities are checked against TI documentation and SDK. Upstream conditional features remain conditional; no capability is required merely because it appeared in an external product architecture. Features not proven feasible for a particular AM13E board remain capability/board integration questions, not silently removed from the upstream scope.

**In scope:** repository/module boundaries; Application/Bootloader source interfaces; fixed single-Application ABI; runtime input, configuration, telemetry and service boundaries; Flash ownership; MCU-specific Build/Startup/Linker specifications. **Out of scope:** modifying code, building firmware, Mock HAL, board measurements, motor testing and A/B/rollback mechanisms.

## 2. ESCape32-native repository and single Application

```text
ESCape32/
├── CMakeLists.txt                 # retain original add_target() pattern
├── src/                           # retain upstream six-step / housekeeping / protocols
├── mcu/
│   ├── STM32*/ AT32*/ GD32*/      # retain all original targets
│   └── AM13E/               # NEW Application config.c/h/cmake/ld + native implementation
└── boot/
    ├── CMakeLists.txt             # retain original boot add_target() pattern
    ├── src/                       # retain upstream command parser and framing
    └── mcu/
        ├── STM32*/                # retain original targets
        └── AM13E/           # NEW Bootloader config.c/h/cmake/ld + native code
```

There is **one ESCape32 rel17 Application**, installed at a **single fixed `APP_BASE`**, and one ESCape32-derived Bootloader. Preserve the rel17 command parser, supported input/telemetry modes, configuration and service mechanisms according to their existing source branches; do not add an unrelated service-tool or telemetry requirement. The address-window extension is the minimum additional Boot protocol capability selected for the larger Flash-layout-derived Application address range; it does not replace or disable any upstream command. The Bootloader owns image programming, retained Boot self-update/protection commands and direct launch; the Application preserves its upstream motor-safe reset/entry behavior.

**Porting invariant:** no original rel17 feature, compile-time branch, command or external protocol may be removed or disabled as an AM13E design shortcut. Rel17 feature availability remains governed by its own target/board compile guards and hardware requirements. Do not introduce A/B, bank swapping, rollback or unrelated features that do not exist in upstream rel17.

## 3. Flash partition and ownership (architecture plan)

The AM13E23019 has 512 KiB MAIN Flash, Bank0 `0x00000000..0x0003FFFF` and Bank1 `0x00040000..0x0007FFFF`. Use **2 KiB** physical erase-sector granularity as the device-specific design basis; inconsistent generic 1 KiB TRM references remain a **TI documentation review** item.

| Region | Start inclusive | End exclusive | Size | Ownership |
|---|---|---|---:|---|
| ESCape32-derived Bootloader | `0x00000000` | `0x00004000` | 16 KiB | Boot code/vector; preserved by normal update |
| ESCape32 parameters | `0x00004000` | `0x00005000` | 4 KiB | Application configuration; preserved during Application reflash |
| Reserved | `0x00005000` | `0x00006000` | 4 KiB | No current writer/reader; preserved and inaccessible in normal service |
| ESCape32 Application | `0x00006000` | `0x00080000` | 488 KiB | Sensorless six-step Application; normal update target |

`APP_BASE = 0x00006000`. These are **selected planning limits** for the linker and firmware update address map; image size, vector relocation and TI SDK feasibility are not asserted as verified. The 4 KiB **Reserved** interval preserves the fixed Application base without defining an additional firmware feature or parameter-storage API. It is not available for Application configuration writes or routine firmware updates. Any future allocation change must update both Bootloader and Application linker/ownership definitions together. An Application may span Bank0 and Bank1 without using A/B images or bank swapping. Normal updates erase/program/verify **only** the Application region and preserve Bootloader, ESCape32 parameters and Reserved. NONMAIN is not an Application parameter store.

**Parameter persistence:** `savecfg()`, `resetcfg()`, `checkcfg()` and `Cfg` preserve their ESCape32 source-level semantics, using the dedicated 4 KiB parameter region (two 2 KiB physical sectors). Validity/defaults/migration, record format and wear/update method remain **Detailed Design** within the existing configuration behavior. The upstream parameter marker **is used by the original Bootloader as its launch gate**, but does not prove that an interrupted Application update left complete executable code.

**Flash execution rule:** when erase/program targets a bank containing executing code, use **SRAM-resident Flash-controller command execution** per TI restrictions. This applies to Bootloader writes to the Bank0 portion of the Application, and to parameter updates when the Application executes from Bank0. The former Bank1-only-App/cross-bank-only assumption is **superseded**. Authorize physical write/erase ranges before unlock, erase or program. Hardware write-protection grouping must account for the 4 KiB parameter region adjacent to the 4 KiB Reserved interval; a 2 KiB erase sector is not necessarily an independently protectable hardware group.

## 4. ESCape32-derived Bootloader — complete command preservation with additive addressing

**Normative preservation rule:** the AM13E port retains all **six upstream rel17 Bootloader commands, numeric IDs, request framing, reply sequencing and functional intent**. Preserve their original C entry points (`recvval()`, `sendval()`, `recvdata()`, `senddata()`, `write()`, `update()`, `setwrp()`), modifying only target-dependent Flash, protection, address and reset operations. In particular **`CMD_UPDATE` and `CMD_SETWRP` are implemented design obligations, not rejected stubs**.

| Rel17 command | ID | Original rel17 protocol behavior to retain | Necessary AM13E adaptation |
|---|---:|---|---|
| `CMD_PROBE` | 0 | No extra argument; returns `sendval(RES_OK)` | None beyond transport |
| `CMD_INFO` | 1 | `senddata()` of 32 bytes: revision, `IO_PIN`, 32-bit MCU identifier, remaining zero | Keep 32-byte framing; use actual AM13E device identity and configured I/O pin; do not forge STM32 identity |
| `CMD_READ` | 2 | 8-bit block argument, 8-bit count; returns `(count+1)*4` bytes as one CRC32-framed `senddata()` response | Same wire format; translate block to new APP origin and selected high window |
| `CMD_WRITE` | 3 | 8-bit block, `recvdata()` up to 1 KiB, then `sendval(RES_OK/RES_ERROR)` | Same framing/reply; authorize APP only, adapt 1 KiB logical blocks to 2 KiB sectors, preserve neighboring data |
| `CMD_UPDATE` | 4 | Receive up to Boot size in 1 KiB `recvdata()` blocks; `RES_OK` acknowledgement after each accepted block; `update()` writes Boot then **reboots on success**; `RES_ERROR` is sent only if update returns/fails | Keep its complete **Bootloader self-update** capability via RAM-staged image and SRAM-resident Flash critical path; original command sequence unchanged |
| `CMD_SETWRP` | 5 | One `recvval()` option: `0x33` Off, `0x44` Bootloader, `0x55` Full; `setwrp(0/1/2)` applies protection and triggers an option reload/reset on success, otherwise handler returns `RES_ERROR` | Implement an AM13E write-protection policy mapping that preserves all three behaviors, including the required persistent/reload semantics; use TRM static protection/NONMAIN procedure only when reversible under the selected security policy |

The rel17 `REVISION=4`, response codes `RES_OK=0`, `RES_ERROR=1`, complement-coded value pair, CRC32 block payload, 1 KiB data block and 4-byte length quantum remain unchanged. **Do not replace the six commands with a new update protocol.** An old Host that assumes the previous combined Config-prefix image *address layout* must adapt to the selected new Flash layout; the command **wire shapes** remain compatible.

**Minimum additional command:** `CMD_WINDOW = 6` selects a high 256-block window (`0` or `1`) for subsequent original `CMD_READ`/`CMD_WRITE`, using the same `recvval()` complement encoding and `sendval(RES_OK/RES_ERROR)` response. `effective_block = 256*window + legacy_block_byte`; legal Application blocks are `0..487`; physical address is `APP_BASE + effective_block*1024`. Initialize window to zero at Boot entry. **No extension changes** the framing or ID of an original command. New Hosts must first identify the AM13E bootloader via existing `CMD_INFO` before sending `CMD_WINDOW`; an upstream old Bootloader may interpret an unknown command as an attempt to launch its Application.

**Flash-layout exception:** rel17 originally checks the `0x32EA` configuration marker at its `_rom_end` and jumps to vectors at `_rom_end + PAGE_SIZE`. AM13E retains the original `Cfg.id` marker at `CFG_BASE=0x00004000` and the original check's meaning, but redirects the vector launch to the fixed `APP_BASE=0x00006000`. Application programming is confined to `[APP_BASE, APP_END)`; preserve Boot/Config/Reserved intervals when updating. Do **not** make a newly invented signed manifest, image CRC or A/B scheme a mandatory upstream feature. Range and basic Cortex-M33 stack/vector eligibility checks are target-safety adaptations of the relocated application entry, not a new application protocol. A configuration marker alone cannot establish that an interrupted APP update left a complete image; this **existing source limitation** is documented rather than falsely claimed solved.

**`CMD_UPDATE` ownership:** collect the same framed Boot image into a specifically allocated, bounded SRAM staging region (Boot target size: 16 KiB), preserving the upstream per-block acknowledgement, short-last-block and success-reset behavior. The AM13E `update()` implementation performs guarded Boot-region-only programming from SRAM when Bank0 cannot be fetched, verifies the written bytes and then resets. Its RAM and vector/interrupt ownership shall be fixed in the Boot linker integration. Error returns preserve the upstream `RES_ERROR` path. It may not erase CFG, Reserved or APP.

**`CMD_SETWRP` ownership:** implement the original Off / Boot-only / Full settings with TI-compatible persistent write protection, respecting the AM13E TRM's distinction between runtime **dynamic** protection and boot-latched **static** protection. For Boot-only, protect exactly `[0x00000000,0x00004000)` without preventing ESCape32 configuration writes. For Full, protect MAIN Flash at the device-supported granularity; Off must remove the reversible policy through its documented update/reload sequence. The port must **not** silently substitute ephemeral dynamic masks for persistent protection, nor permanently lock NONMAIN/security configuration so that the original Off command can never restore writes. If the selected production security configuration cannot meet this reversibility, the incompatibility is a **design blocking condition to resolve**, *not permission to delete CMD_SETWRP*. The precise reset/reload and authorized NONMAIN routine require device-specific review; generic `DL_Flash_program()` is not a NONMAIN programming API.

**AM13E Flash geometry:** each original 1 KiB read/write packet maps to an AM13E 2 KiB physical sector through bounded read-modify-erase-program and 16-byte-aligned programming buffers as required by the supplied SDK implementation. APP starts in Bank0 (like Boot) and continues into Bank1; Bank0-targeting Boot code must run its busy/erase/program critical path safely from SRAM. Dynamic protection must be reconfigured for **each** erase/program operation, since TRM protection masks reset to protected afterward. Enforce region bounds before any unlock or erase, not merely at the Host parser.

**Command support criterion:** `CMD_PROBE/INFO/READ/WRITE/UPDATE/SETWRP` are all marked **MUST PRESERVE** in the mapping and interface specification. New commands may only extend necessary AM13E functionality, never supersede originals. This is a design requirement; it does not claim physical/programming verification has already occurred.

## 5. ESCape32 rel17 feature and control boundary — source is authoritative

The ESCape32 Application retains its existing `nextstep()`, `compctl(pcc); pcc=cc`, BEMF timing/filter algorithm, input-mode selection, DShot command parser, telemetry, `savecfg()`/`resetcfg()` and other upstream behavior. Only target-specific register, peripheral, interrupt, startup and Flash operations are isolated for AM13E. Do not change input priority, enable a feature, or drop an upstream option merely to match a separate system document.

### Source-present behavior (not external requirements)

| Rel17 source behavior | Evidence in upstream | Porting treatment |
|---|---|---|
| Servo PWM / Oneshot125 input, calibration | `src/io.c`: `entryirq()`, `calibirq()`, `servoirq()`; `README.md` | Preserve its mode selection/validation and throttle policy; replace MCU timer capture with a native receiver adapter where that mode is enabled |
| DShot 300/600/1200 receive | `src/io.c`: `dshotirq()`, `iotim_dma_isr()`, `dshotcrc()`; `README.md` | Preserve protocol and command logic, replace timer/DMA capture |
| Bidirectional DShot and extended telemetry | `src/io.c`: `dshotinv`, DMA direction-change handling, GCR table; `src/telem.c`: `dshotval`/extended telemetry | Preserve the **existing conditional RX/TX timing pipeline** if the corresponding rel17 input mode and board capabilities enable it; it is not independently mandated |
| Analog/serial/iBUS/SBUS/SBUS2/CRSF/EXBUS/HoTT inputs | `src/main.c` `input_mode`; `src/io.c` `serialirq()`/decoders; `README.md` | Keep upstream selection/compile guards; provide AM13E UART/ADC/timing adaptation when selecting such a target configuration, not a new parser |
| KISS/iBUS/S.Port/CRSF/MSB/HoTT telemetry | `src/telem.c`, `README.md` | Preserve protocol formatting; adapt native transport only for enabled modes |
| Sine startup, brushed, hybrid/Hall, braking and sound | `src/main.c`, `src/util.c`, `README.md` | Retain source behavior/flags. Any mode with different motor-drive semantics needs its own native adapter mapping; it must **not** be silently mapped to basic six-step |
| LEDs, beacon, parking, BEC, ERPM outputs | `src/main.c`, `src/io.c`, MCU `config.c`, target macros | Conditional board and GPIO features; preserve source gates and define backend when selected |
| ADC-based voltage/current/temperature protection | `src/main.c` `adcdata()`, `src/defs.h` `SENS_MAP`, board `config.c` | Preserve existing `adcdata()` ordering and source scaling; only configured sensor channels exist. Do not invent a separate current stream or mandate external extra sensing |

**Key distinction:** `PWM_ENABLE` in `src/main.c` is a **motor-output compile option** affecting timer output controls, **not** the Servo PWM receiver mode. `SENS_MAP` omitted (rel17's default) is not a statement about any outside product sensing requirement. Target defaults must be derived from rel17 and actual board wiring, not copied from an unrelated baseline.

**DShot ordering:** in rel17 `iotim_dma_isr()`, the inverted-mode physical reply setup occurs in the DMA handling path **before** the current received packet has completed its timing/CRC disposition; a subsequent DMA-completion branch prepares the response payload. Preserve this original response pipeline and telemetry latency. Do not rewrite it as a generic "CRC-valid packet → then start reply" sequence. A native adapter must prevent invalid command application, keep RX/TX ownership exclusive, and preserve the source-specific resynchronization behavior. See [Interface_Contracts.md](Interface_Contracts.md).

**BEMF invariant:** retain delayed `compctl(pcc)` selection, upstream zero-cross rejection and schedule timing relative to the original ISR handler epoch. MCPWM/CMPSS/eCAP are native backend candidates, not new core algorithms. Exact pins, comparator paths and gate-driver electrical truth table remain board-specific.

## 6. Configuration, telemetry and service boundary

The service, command and telemetry capabilities are only those present in rel17 and enabled by its existing compile/runtime configuration. Reuse `src/prog.c`, `src/telem.c`, `src/io.c` and existing MCU hooks. Do not add proprietary service-tool metadata, additional telemetry channels, a mandatory single-pin multi-protocol constraint, or a new firmware feature merely to match another architecture.

The existing source-level reset/boot service path must be adapted to Cortex-M33 with safe outputs and the previously selected fixed `APP_BASE`. ADC channel existence, current/voltage/temperature scaling and input line electrical ownership are target/board facts, not inferred product requirements.

## 7. Build and source ownership

Continue the native `add_target(name mcu ...)` selection for the **ESCape32 Application** and **Bootloader**. Add `mcu/AM13E/{config.c,config.h,config.cmake,config.ld}` and `boot/mcu/AM13E/{config.c,config.h,config.cmake,config.ld}`; native implementation files within these directories own hardware differences. Do not replace the existing structure with `core/platforms/boards` directories. 

AM13E targets use one Cortex-M33 Startup/Vector definition and one image-specific Linker Script each; exclude libopencm3/ST-only definitions, `st-flash`, duplicate `main`/Reset vectors and generic STM32 Flash-size assumptions. Rel17's `void main(void)` versus TI startup's `int main(void)` requires an AM13E-only entry bridge. Share constants for `BOOT_BASE`, `CFG_BASE`, `RESERVED_BASE`, `APP_BASE`, region ends and sector size **between** Boot/App target configurations. Linker-bound assertions ensure Boot ≤16 KiB and APP is within its region; actual binary size is an implementation result.

## 8. Design status and update rules

| Topic | Disposition |
|---|---|
| ESCape32-native Application and Bootloader source ownership | **Selected design** |
| One fixed Application region; 4 KiB ESCape32 parameters; 4 KiB Reserved | **Selected architecture plan** |
| Input/telemetry behavior from rel17 | **Source-backed modes; selected through existing configuration and target capabilities, not imposed as mandatory product features** |
| 16 KiB Bootloader + 4 KiB configuration + 4 KiB Reserved + 488 KiB Application | **Selected Flash-layout plan; linker fit not asserted** |
| `CMD_WINDOW=6` for the complete 488 KiB Application | **Additive Flash-layout addressing extension**, not upstream; service host must support it |
| Image validity | **Retain upstream `Cfg.id=0x32EA` marker check**, relocated vector/stack checks only; interruption completeness is a known upstream limitation, not solved by introducing a new image manifest |
| Physical peripherals, pins, gate driver and analog conditioning | **Board hardware configuration ownership** |

Source-based mappings are maintained by **function/module identity and behavior**, not source line numbers. The Flash addresses and `CMD_WINDOW` are *AM13E integration choices*, not features attributed to upstream rel17. These three Markdown files are the only maintained deliverables; no source evidence dumps, change-plan spreadsheets, external HAL framework or invented header are required. Design-only acceptance excludes executable/Mock tests and physical validation.


## 9. Full-feature preservation and remaining target decisions

Application control algorithms, all upstream command/telemetry formats, conditional input methods, Hall/hybrid, sine, brushed, motor audio, GPIO-controlled options, watchdog/failsafe and configuration persistence are inherited **without deletions**. Any physically conditional feature must be representable by the native board/MCU adapters described in the companion Mapping and Contracts. An AM13E target does not need to enable incompatible upstream board configurations simultaneously, exactly as upstream targets do not.

**Not yet a hardware feasibility finding:** the actual gate-driver output truth table, all mutually exclusive peripheral assignments, exact TI NONMAIN protection update/reset sequence and selected board pins are configuration/target integration inputs. In particular `CMD_UPDATE`/`CMD_SETWRP` preservation is mandatory design scope even where low-level implementation details require TI SDK/TRM confirmation. These are not reasons to remove an upstream feature.
