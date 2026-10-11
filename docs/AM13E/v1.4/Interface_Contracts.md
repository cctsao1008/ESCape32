# ESCape32 rel17 to AM13E23019 — Interface Contracts

**Revision:** 1.4 — full rel17 feature preservation, original Boot command contracts and additive Flash addressing; no `.h` files or executable port have been added.  
**Cross-reference:** [Integration_Mapping.md](Integration_Mapping.md); architecture and partition in [Integration_Design.md](Integration_Design.md).  
**Count:** **26 unique new private function names** = 16 ESCape32 Application services (`A01`–`A16`) + 5 ISR-to-Application callbacks (`C01`–`C05`) + 5 Bootloader operations (`B01`–`B05`). Boot `B06` is the **same signature** as Application `A13`, linked separately in Boot. Existing ESCape32 hooks are **not** counted. These are a design seam, **not** a replacement generic 99-API HAL. Existing `write()`, `update()` and `setwrp()` require full AM13E implementations but are **reused upstream interfaces**, not counted as newly invented HAL APIs.

## 1. Existing ESCape32 rel17 interfaces — reuse without renaming

```c
#include <stdint.h>

/* ESCape32 Application, existing upstream signatures */
void init(void);
void initio(void);
void initgpio(void);
void initled(void);
void ledctl(int x);
void compctl(int x);
void adctrig(void);
void adcdata(int t, int u, int v, int c, int a);
void io_serial(void);       /* rel17 input_mode / board-selected */
void io_analog(void);       /* rel17 optional analog input */
void checkcfg(void);
int  savecfg(void);
int  resetcfg(void);
int  execcmd(char *str);

/* Boot, separately linked in its own firmware image */
void     init(void);
void     initio(void);
int      recvbuf(char *buf, int len);
void     sendbuf(const char *buf, int len);
int      recvval(void);
void     sendval(int val);
int      recvdata(char *buf);
void     senddata(const char *buf, int len);
uint32_t crc32(const char *buf, int len);
int      write(char *dst, const char *src, int len);
void     update(char *dst, const char *src, int len);
void     setwrp(int type);
```

`initio()` in the shared ESCape32 source is the existing entry hook for setting up a native receiver; it must not be duplicated by a synonymous `dshot_init()`. `compctl(pcc)` selects the *previously calculated* BEMF phase candidate exactly as in rel17; no second logical `select_comparator()` is needed. `adctrig()` triggers the board's defined sensing set, with `adcdata()` preserving its existing parameter ordering and logic. Normal firmware-entry-to-Boot reset uses the existing service request flow after safe output shutdown, not a new mandatory motor HAL framework.

## 2. Proposed signatures

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    E32_AM13E_IDLE_LOW_SIDE_SHORT = 0,
    E32_AM13E_IDLE_ALL_OFF = 1
} e32_am13e_idle_t;

enum {
    E32_AM13E_RESET_POR = 1u << 0,
    E32_AM13E_RESET_WATCHDOG = 1u << 1,
    E32_AM13E_RESET_SOFTWARE = 1u << 2
};

/* ESCape32 Application -> AM13E private motor/system backend */
int      e32_am13e_bridge_stage_step(uint8_t source_mask, uint8_t sink_mask, bool damped);  /* A01 */
int      e32_am13e_bridge_set_pwm(uint32_t period_ticks, uint32_t compare_ticks);          /* A02 */
int      e32_am13e_bridge_idle(e32_am13e_idle_t mode);                                     /* A03 */
void     e32_am13e_bridge_force_off(void);                                                 /* A04 */
int      e32_am13e_bridge_commit(void);                                                    /* A05 */
void     e32_am13e_bridge_commutation_enable(bool enabled);                                /* A06 */
int      e32_am13e_bemf_arm(uint8_t filter_level, uint32_t timeout_rel17_ticks);            /* A07 */
int      e32_am13e_bemf_schedule_from_handler(uint32_t delay_rel17_ticks);                 /* A08 */
void     e32_am13e_bemf_disarm(void);                                                      /* A09 */
int      e32_am13e_tick_start(uint32_t rate_hz);                                           /* A10 */
void     e32_am13e_watchdog_start(void);                                                   /* A11 */
void     e32_am13e_watchdog_feed(void);                                                    /* A12 */
uint32_t e32_am13e_reset_cause(void);                                                      /* A13 / Boot B06 */
void     e32_am13e_fatal_reset(void) __attribute__((noreturn));                            /* A14 */
int      e32_am13e_config_store(const void *ram_config, size_t bytes);                    /* A15 */
int      e32_am13e_bidir_transmit(uint32_t encoded_gcr20);                                /* A16 */

/* AM13E timing/ISR backend -> ESCape32 Application callbacks */
void     e32_app_on_commutation_due(void);                                                 /* C01 */
int      e32_app_on_bemf_capture(uint32_t elapsed_rel17_ticks);                            /* C02 */
void     e32_app_on_bemf_timeout(void);                                                     /* C03 */
bool     e32_app_on_dshot_capture(const uint16_t intervals[32],
                                  uint16_t two_bit_ticks, bool inverted);                  /* C04 */
bool     e32_app_on_pwm_pulse(uint32_t high_us, uint32_t period_us);                        /* C05 */

/* ESCape32 Bootloader -> AM13E private Boot backend */
int      e32_am13e_boot_read_block(uint16_t effective_block,
                                    uint8_t *dst, uint16_t bytes);                          /* B01 */
int      e32_am13e_boot_write_block(uint16_t effective_block,
                                     const uint8_t *src, uint16_t bytes);                   /* B02 */
uint32_t e32_am13e_boot_device_id(void);                                                   /* B03 */
bool     e32_am13e_boot_application_valid(void);                                           /* B04 */
void     e32_am13e_boot_jump_to_application(void) __attribute__((noreturn));               /* B05 */
/* B06: same e32_am13e_reset_cause() declaration, independently linked in Boot. */
```

**ABI caution:** these are *proposed design* signatures, not declarations found in ESCape32 or in TI DriverLib. No source file has been modified. `bool`, `size_t`, integers and `noreturn` requirements must be supported by the selected ARM toolchain before use in compiled code.

## 3. Bridge, PWM, BEMF and event contracts (`A01`–`A10`, `C01`–`C03`)

| ID | Contract |
|---|---|
| `A01` | One logical phase SOURCE_PWM, one different phase SINK_LOW and one floating; masks bit0=A, bit1=B, bit2=C. `damped` reflects rel17 choice; each rel17-selected damping mode requires an equivalent native output truth table; absent physical support is a board-design blocker, not permission to remove the upstream mode. Stage coherently, not per-FET writes. |
| `A02` | Apply logical carrier period/compare requests after mapping rel17 timer units to actual MCPWM clock; bounds-check and reject unsupported duty/frequency. |
| `A03` | Distinguish rel17 idle low-side-short intention from true all-output-off. Physical output truth table, fault trip and gate polarity come from the board configuration. |
| `A04` | All outputs safe-off, cancel pending drives and keep outputs disabled through the Boot-entry/fatal safety transition. Hardware fast trip is independent of ISR response. |
| `A05` | Commit pre-staged phase/duty as one coherent update, equivalent to rel17 COM/update behavior, never partially energize. |
| `A06` | Disable/cancel pending commutation interrupts without replay; enable only a valid current-generation due event. |
| `A07` | Arm BEMF capture/filter/timeout **after** existing `compctl(pcc)` selection; preserve rel17 filter/timeout bands and half-period rejection. Comparator phase path is board-defined; do not overlap independent overcurrent-trip resources. |
| `A08` | On accepted zero crossing, reset schedule timebase at handler acceptance (source `TIM_EGR=UG` semantic), disarm old capture and schedule exactly one next due. Do **not** silently use hardware capture timestamp as epoch. |
| `A09` | Cancel both capture/overflow and due events; prevent stale callback delivery after stop/rearm. |
| `A10` | Start source-equivalent 16 kHz tick **at the selected rel17 initialization point**, not prematurely inside `init()`. |
| `C01` | Native IRQ notifies shared code exactly once when a current-generation commutation deadline expires; the shared six-step algorithm calls `nextstep()`. |
| `C02` | Shared BEMF ISR math returns `<0` invalid, `0` early/rejected (`t < ival/2`, remain armed), `1` accepted and successfully scheduled via `A08`; preserve `ival`, `fast`, `sync`, `cfg.timing`. |
| `C03` | Timeout takes precedence if timeout and capture pending simultaneously; shared code restores rel17's timeout/sync state. No accepted `C02` may follow for the same event generation. |

All negative adapter returns retain safe outputs and cause a coherent fault/refusal path. Logical phase roles are **not** a physical GH/GL polarity specification. This phase-role contract covers only the ESCape32 sensorless six-step control path.

## 4. Upstream rel17 input-mode and inverted-DShot contracts (`A16`, `C04`, `C05`)

### Receiver mode and same-line arbitration

`initio()` and rel17 `entryirq()` implement an input-timer path that may identify Servo/Oneshot or DShot under `cfg.input_mode=0`. Other upstream `input_mode` configurations select analog and serial receiver paths. **Do not impose a three-protocol always-on device contract** or assume two separate command pins. The native event backend is responsible only for the hardware capture, DMA, line direction and re-arm operations needed by the *selected rel17 path*. Preserve source mode transitions, timeout/failsafe and watchdog criteria.

`C05(high_us,period_us)` conveys one native Servo/Oneshot pulse observation into a rel17-equivalent `calibirq()`/`servoirq()` path, when that source mode is selected. The upstream code owns `setthrot()`, special pulse handling and calibration. Its boolean return is a proposed adapter disposition only; it must not introduce a new pulse range or failsafe rule.

`C04(intervals[32],two_bit_ticks,inverted)` supplies one stable rel17-style capture to shared bit classification, `dshotcrc(x,inverted)`, throttle/command and telemetry-value logic. The proposed boolean return reports frame timing/checksum acceptance **only**, not response-transmit completion. Keep `inverted` based on the source-selected inverted mode. The buffer must not change while decoded; invalid frames must not modify throttle/command state.

`A16(encoded_gcr20)` is the native physical waveform/turnaround primitive used **only when rel17 enters inverted bidirectional DShot mode**. `src/io.c` owns the GCR table and telemetry content. In the *actual rel17 DMA ISR*, receive completion selects the inverted reply/TX path before validating the current frame; later TX DMA completion calls `dshotreset()` and prepares the next response payload from `dshotval`/`ertm`. The adapter must preserve this pipeline and source latency; it must not gate physical reply initiation solely on the `C04` acceptance result. The existing RX resynchronization path (`dshotresync()`/`dshotreset()`) must have one owner across shared code and backend, avoiding duplicate resets. A receive callback is not equivalent to TX completion or RX readiness. The existing ~30 us delay is a source timing reference, not an external protocol addition.

No new DShot command parser or second physical command pin is implied. Detailed TX waveforms and physical pin realization are board/implementation-owned.

## 5. Runtime monitoring, reset and firmware identification

| Service / symbol | Contract |
|---|---|
| `adctrig()` / `adcdata(t,u,v,c,a)` | Reuse rel17 callback and upstream voltage/current/temperature processing only for configured source channels (`SENS_MAP`, temperature/analog options). Board-specific analog scaling is required for any enabled channel; no extra measurement is mandated. |
| `A11`, `A12` | Watchdog start/feed retains source-qualified input/failsafe paths; a DMA interrupt or invalid frame alone does not prove a valid command. |
| `A13` / `B06` | Same reset-cause bit-domain signature, different code in Application/Bootloader images; do not reuse STM32 `RCC_CSR` bit patterns. |
| `A14` | Non-returning fatal reset after safe motor shutdown; normal service-to-Boot entry uses safe stop followed by a supported Cortex-M33 software reset. |
| `src/prog.c`/service behavior | Preserve the rel17 commands, parameter names and service transport behavior. Do not add a version-discovery protocol or new external service requirement as part of the MCU port. |

## 6. Persistent ESCape32 parameters (`A15`) and fixed Application ABI

**Partition constants (end-exclusive):**

```text
BOOT       [0x00000000, 0x00004000)   16 KiB
CFG        [0x00004000, 0x00005000)    4 KiB
RESERVED   [0x00005000, 0x00006000)    4 KiB
APP        [0x00006000, 0x00080000)  488 KiB
SECTOR_SIZE = 0x800 (2 KiB, selected device-specific basis)
```

`A15(ram_config,bytes)` is **Application-only by construction**: it is linked into the ESCape32 Application and hard-authorizes only `[0x00004000,0x00005000)`. There is no address/region selector, and `[0x00005000,0x00006000)` is **Reserved** and never used for normal configuration or update transactions. The 4 KiB configuration interval contains two 2 KiB physical sectors. `savecfg()` retains its existing `int` and motor-stop/busy refusal behavior; it returns `1` only after the authorized configuration transaction completes and verifies, `0` otherwise. Exact parameter-record CRC, interrupted-write recovery and wear handling are **Detailed Design**, not presumed existing behavior.

On startup the Application checks compatibility/validity of its parameter data before restoring runtime `cfg`; otherwise it uses existing `cfgdata` factory defaults and `checkcfg()`. Preserve `resetcfg()` semantics. Linker/SDK integration must prohibit Application update, Boot commands and configuration writes from modifying the Reserved interval. Any erase/program command that targets a bank currently supplying executing instructions must use a supported **SRAM-resident Flash command sequence**; the Application may execute from Bank0. Sector and protection-group authorization occurs *before* controller unlock/erase/program.

The installed ESCape32 Application obeys fixed `APP_BASE=0x00006000`, Cortex-M33 vector/stack/reset contract and update-request reset behavior. The 4 KiB configuration and 4 KiB Reserved intervals are preserved across reflashing. The upstream Bootloader uses the `Cfg.id=0x32EA` marker to permit a jump; this is a source-compatible marker check, **not** a proof of complete-image integrity after interrupted programming. No additional mandatory image manifest or commit protocol is introduced.

## 7. Bootloader contracts — preserve all six upstream commands

**Upstream rel17 commands remain complete functional obligations; AM13E may only extend the Flash-layout address range.** Source command handler `boot/src/main.c`, complement-code and CRC32 framing in `boot/src/io.c`, and source-level `write()`/`update()`/`setwrp()` entry points remain normative. The original MCU-specific bodies may be conditionally excluded for AM13E, but their **externally observable functions are not deleted**.

```c
/* Exact existing rel17 numeric command IDs */
#define CMD_PROBE  0
#define CMD_INFO   1
#define CMD_READ   2
#define CMD_WRITE  3
#define CMD_UPDATE 4
#define CMD_SETWRP 5
#define RES_OK     0
#define RES_ERROR  1

/* AM13E-only addressing extension; absent from rel17 */
#define CMD_WINDOW 6

/* Existing upstream Boot function signatures, AM13E implementations */
int  write(char *dst, const char *src, int len);
void update(char *dst, const char *src, int len);
void setwrp(int type);
```

### 7.1 Original wire format and replies

| Command | Input | Original output / completion behavior | Required target treatment |
|---|---|---|---|
| `CMD_PROBE` | command only | `sendval(RES_OK)` | Preserve |
| `CMD_INFO` | command only | `senddata(buf,32)` where `buf[0]=REVISION` (`4` in rel17), `buf[1]=IO_PIN`, `buf[2..5]` MCU ID, rest zero | Preserve **32 bytes**, fields and CRC; use native identity, no fake STM32 ID |
| `CMD_READ` | 8-bit block index plus 8-bit count via `recvval()` | `senddata()` of `(count+1)*4` bytes | Preserve length and CRC framing; only Flash-layout-based source address changes |
| `CMD_WRITE` | 8-bit block index plus `recvdata()` CRC-protected payload | `sendval(RES_OK)` on successful write/verify, `RES_ERROR` otherwise | Preserve response; 1 KiB packet, 2 KiB sector RMW |
| `CMD_UPDATE` | Up to `(BOOT_SIZE/1024)` blocks via `recvdata()`; stop at a short last block | `sendval(RES_OK)` **after each accepted block**; on successful `update()` the device resets and does **not** return a final success value; a returning failed update reaches `sendval(RES_ERROR)` | Must implement **Bootloader self-update**, not reject/skip. Use bounded 16 KiB SRAM staging and SRAM-resident bank-conflicting Flash writes, verify, then reboot |
| `CMD_SETWRP` | One 8-bit option: `0x33` Off / `0x44` Boot-only / `0x55` Full | Valid `setwrp()` changes policy and causes required reset/reload on success; if it returns/fails, dispatcher sends `RES_ERROR` | Must implement all three choices, including persistent/reload behavior and reversible Off mode. Unknown option retains rel17 error path |

If the serial framing sees a bad complement value, wrong CRC or short transport read, maintain the upstream failure/exit path. The `recvval()` signed `char` portability issue must be handled in the AM13E-specific framing implementation with unsigned octet arithmetic **without changing** accepted wire bytes; specifically `0x33`, `0x44`, `0x55` and high block indices are still valid.

### 7.2 Flash layout / address-window extension

```text
BOOT       [0x00000000,0x00004000) 16 KiB
CFG        [0x00004000,0x00005000)  4 KiB
RESERVED   [0x00005000,0x00006000)  4 KiB
APP        [0x00006000,0x00080000) 488 KiB
FLASH ERASE SECTOR = 0x800 (2 KiB design basis)
```

`CMD_WINDOW=6` accepts one complement-coded window value `0` or `1` and replies with rel17 `sendval(RES_OK/RES_ERROR)`. Initialize window to `0` on each Boot session. Effective 1 KiB block = `256*window + (uint8_t)old_block`; valid blocks `0..487`, physical byte address `APP_BASE + 1024*effective_block`. The final window rejects indices `488..511`. A newly updated service Host must identify AM13E from the **original `CMD_INFO` reply** before sending this extension: unknown commands on old rel17 Bootloaders enter the default launch path. Do not claim that an unchanged old Host can upload every byte in the relocated 488 KiB region. `CMD_WINDOW` never changes the shape of any original command.

| New/private interface | Contract |
|---|---|
| `B01` `e32_am13e_boot_read_block(...)` | Range-check source in APP, original `(cnt+1)*4` data bytes, no memory-mapped read outside permitted APP window; upper-window invalid requests shall exit safely rather than fabricate a valid `senddata()` frame |
| `B02` `e32_am13e_boot_write_block(...)` | Range-check APP-only (never Boot/CFG/Reserved); reject malformed CRC/count before Flash; 1 KiB logical write uses 2 KiB sector snapshot + preservation + erase/program + verify. Original `CMD_WRITE` ACK remains unchanged |
| `B03` `e32_am13e_boot_device_id()` | Authentic AM13E 32-bit information-field identity; `CMD_INFO` retains original 32-byte data frame |
| `B04` `e32_am13e_boot_application_valid()` | Retain the **upstream** `Cfg.id=0x32EA` in CFG at `0x4000` as the original validity marker and check the **relocated** vector at `0x6000` for a Cortex-M33 legal stack/Thumb reset entry. Do **not** require newly invented signed manifests/image CRC/commit protocol as a condition for source compatibility. The upstream marker does **not** prove completeness after interrupted updates |
| `B05` `e32_am13e_boot_jump_to_application()` | Transfer to relocated APP vector/stack after Boot peripherals/interrupts are quiesced. No new firmware type/selector semantics |
| `B06` | Same `e32_am13e_reset_cause()` signature as Application `A13`, separately linked in Boot; preserve the upstream reboot-ACK intention |

The marker-to-vector offset changes **only** because of the selected Flash layout. The marker is not newly created; it is the existing ESCape32 `Cfg.id` placed at the new CFG address. Do not confuse `PAGE_SIZE` or generic sector size with the explicit `APP_BASE` vector location.

### 7.3 Existing `write()` / `update()` / `setwrp()` implementation contracts

| Existing upstream function | AM13E responsibility and execution semantics |
|---|---|
| `write(dst,src,len)` | Native Flash backend has a bounded, selected-region implementation. Normal `CMD_WRITE` accesses only APP through B02; Boot self-update uses a dedicated explicit Boot-region authorization and may not be achieved by trusting an arbitrary raw pointer. Keep original `int` success/failure convention and the source's data verification intent. Do not write CFG/Reserved through normal Boot update commands |
| `update(dst,src,len)` | Implement the original Boot self-update operation for `dst=BOOT_BASE`, bounded at 16 KiB. Collect the complete Boot image in reserved SRAM (no staging overwrite of live stack/data), use SRAM-resident Flash critical operations for Bank0, verify and issue native reset on success; if failed, return so the rel17 handler sends `RES_ERROR`. Preserve per-`recvdata()` ACKs produced by the original parser. This method shall not erase APP/CFG/Reserved |
| `setwrp(type)` | `0`: Off (reversible writable MAIN policy), `1`: protect Boot region only, `2`: Full MAIN protection. Treat rel17 `setwrp()` reset/reload-on-success and error-on-return as normative. Realize with AM13E **static** WRP/NONMAIN configuration when persistence is required, plus dynamic protection for individual Flash operations; **do not** equate transient `CMDWEPROT*` masks with persistent source WRP. Do not irreversibly lock NONMAIN under policies that must later accept `type=0` |

**Protection distinction from TRM Chapter 13.4:** static protection is boot-latched and configured using NONMAIN; dynamic protection is runtime, resets protected after commands, and cannot by itself satisfy the upstream persistent Off/Boot/Full command. Boot 16 KiB boundary is protectable within Bank0's first-32-KiB region. Later Bank0/B1 Flash has coarser protection groups; protect exactly source-required regions to the nearest valid group without accidentally blocking Config writes under Boot-only mode. Device security/lifecycle state and reset type required to apply new static protection are target prerequisites requiring precise TI confirmation. If reversible WRP cannot be realized under the selected security settings, **the port design is blocked at this interface rather than silently removing CMD_SETWRP**. `DL_Flash_program()` rejects NONMAIN and shall not be repurposed as its configuration API.

**Flash safety and RWW:** App starts in Bank0 alongside Boot and continues into Bank1. Use SRAM Flash command execution when targeting currently executing Flash Bank; authorize address bounds before unprotect/erase, verify per sector, and restore dynamic write protection after every operation. The SDK's `DL_Flash_program()` implementation checks 16-byte flash-address alignment; its header wording differs. 1 KiB Host blocks are **not** 2 KiB erase commands, so one Host write must not erase the neighboring 1 KiB.

### 7.4 Command/functional preservation rule

**Source-equivalent interface** means all original 0..5 commands still function; retaining an opcode that merely returns `RES_ERROR` is **not** preservation. Adding `CMD_WINDOW=6` for larger Application addressing is permitted as the explicit Flash-layout exception. No new command may repurpose, rename, drop or redefine any existing ID. The actual TI NONMAIN programming implementation and protection reload/reset must be resolved against the selected SDK/security policy before claiming complete device-level equivalence; executing code or motor tests are outside this architecture-only design stage.

## 8. Feature/Build and implementation-owned choices

- **ESCape32 Application:** the complete *source-defined* feature model stays authoritative, including Servo/Oneshot/DShot, conditional inverted-DShot, analog/serial receiver modes, multiple telemetry protocols, sine/brushed/Hall/hybrid options, braking, audio and target-controlled I/O. No feature is mandatory because an external document listed it; no upstream feature is deleted merely because a reference omitted it.
- **Source ownership:** Application native operations in `mcu/AM13E/`; Bootloader operations in `boot/mcu/AM13E/`. Retain the root/Boot `add_target()` build conventions.
- **Board-owned configuration:** gate-driver truth table, PWM/CMPSS/timer route, conditional sensor/receiver/telemetry pin functions and electrical constraints. Only the hardware needed by each **selected rel17 source branch** is allocated; do not invent pins, protection/telemetry services or mandatory shared-line arrangements.
- **Detailed Design:** target-specific implementation of upstream input detection, inverted-DShot waveform timing, conditional legacy feature branches, Flash/image validity and required host update extension. All upstream feature paths remain design scope. MCU-specific backend capability and compile guards must cover the function before it is claimed executable; never delete a feature to make the interface table smaller.

## 9. Document synchronization rule

Every new `Axx`/`Cxx`/`Bxx` signature here appears in a stable source mapping in [Integration_Mapping.md](Integration_Mapping.md). Existing `init()`/`compctl()`/`adctrig()`/`initio()` hooks remain the first-choice integration points; no private API is required merely for a protocol parser, CRC/PID calculation or an unused ESCape32 hardware variant. When upstream source or TI SDK changes, reconcile by module/function semantics, **not source line numbers**. No separate Evidence directory or `.h` file is mandated.
