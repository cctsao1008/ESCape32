# ESCape32 rel17 → TI AM13E23019 — Interface Contracts

**Revision:** v1.6 — full-feature MCU seam contracts (Application and Bootloader)  
**Source baseline:** ESCape32 rel17, commit `1d718c143380c3eb7e6581478990e1496632e5b8`  
**Companions:** [Integration_Design.md](Integration_Design.md) · [Integration_Mapping.md](Integration_Mapping.md)

## 1. Status, ownership and compatibility

The prototypes below are **proposed AM13E-private design signatures** for implementation planning, **not** upstream ESCape32 definitions or executed firmware. They are consolidated only after inspecting the source's optional drive/input/telemetry/Hall/audio and Boot paths. Each ID is used in the Mapping. The selected port retains all upstream behavior; no new application command, telemetry field, motor algorithm or source feature is introduced.

**Private symbol census:** `A01–A25` (25 Application hardware services) + `C01–C11` (11 ISR→Application event callbacks) + `B01–B05` (5 Boot-private services) = **41 unique proposed names**. `B06` is **the same signature** as `A13`, independently linked in Boot. Existing `init()`, `compctl()`, `initio()`, `adctrig()`, `hallcode()`, `ledctl()`, `write()`, `update()`, `setwrp()`, etc. are *not* new private APIs. The count is a review inventory, not a mandatory API quota. C function definitions/files are selected when porting; source call sites may share a service.

**Core boundary:** `src/main.c`, `src/io.c`, `src/util.c`, `src/telem.c`, `src/prog.c`, and `boot/src/*` own all policy, state, protocol and arithmetic. Target services own only physical timer/capture, gate, analog, serial, DMA, Flash, reset/interrupt and GPIO operations. `mcu/AM13E/` and `boot/mcu/AM13E/` remain **separate binaries**. HAL must not call or parse a new control protocol.

### Source-defined Hooks to preserve (signatures present in rel17)

```c
#include <stdint.h>
/* Existing rel17 Application: implement/retain as selected by its source configuration. */
void init(void);
void initio(void);
void initgpio(void);
void initled(void);
void ledctl(int x);
void compctl(int x);
void adctrig(void);
void adcdata(int t, int u, int v, int c, int a);
void io_serial(void);
void io_analog(void);
void hsictl(int x);
int  hallcode(void);    /* Only when HALL_MAP selects this rel17 path. */
void checkcfg(void);
int  savecfg(void);
int  resetcfg(void);
int  execcmd(char *str);
/* Existing rel17 Boot, in separately linked Boot binary. */
void init(void);
void initio(void);
int recvbuf(char *buf, int len);
void sendbuf(const char *buf, int len);
int recvval(void);
void sendval(int val);
int recvdata(char *buf);
void senddata(const char *buf, int len);
uint32_t crc32(const char *buf, int len);
int write(char *dst, const char *src, int len);
void update(char *dst, const char *src, int len);
void setwrp(int type);
```

Source functions that are weak, conditional or defined in shared source (e.g., `initgpio()`, `hallcode()`, `hsictl()`, `ledctl()`) must have **one** effective definition after AM13E conditional isolation. An AM13E implementation may replace the weak source function, or source may delegate through a private seam under `AM13E`; duplicate strong definitions are prohibited. `initio()` is the *original input/setup name*; it does not justify adding `dshot_init()`.

## 2. Type system and proposed C signatures

The types encode **rel17 logical actions**, not STM32 peripheral register fields. Clock ratios, physical polarity, channel routing, gate safety and IRQ event wiring are properties of the selected AM13E board target.

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    E32_DRIVE_DISABLED = 0,       /* Gate output disabled / high impedance as board permits */
    E32_DRIVE_STATIC_INACTIVE,    /* Logically inactive waveform level */
    E32_DRIVE_STATIC_ACTIVE,      /* Logically active waveform level */
    E32_DRIVE_PWM_ACTIVE_HIGH,    /* Source PWM1-like logical waveform */
    E32_DRIVE_PWM_ACTIVE_LOW      /* Source PWM2-like logical waveform */
} e32_drive_level_t;

typedef struct {
    e32_drive_level_t high_side;  /* Logical phase high-side output */
    e32_drive_level_t low_side;   /* Logical phase low-side output */
} e32_phase_drive_t;

typedef struct {
    e32_phase_drive_t phase[3];   /* A/B/C, preserving upstream COMP_MAP role mapping */
    bool upstream_damped;
    bool output_inverted;         /* Rel17 INVERTED_HIGH build choice */
    bool pwm_enable_variant;      /* Rel17 PWM_ENABLE output-model choice */
} e32_am13e_drive_plan_t;

typedef enum {
    E32_IDLE_LOW_SIDE_SHORT = 0,
    E32_IDLE_COAST_ALL_OFF,
    E32_IDLE_SOURCE_DEFINED_BRAKE
} e32_am13e_idle_t;

enum {
    E32_RESET_POR = 1u << 0,
    E32_RESET_WATCHDOG = 1u << 1,
    E32_RESET_SOFTWARE = 1u << 2
};

typedef enum {
    E32_GPIO_ERPM, E32_GPIO_BEC_BIT0, E32_GPIO_BEC_BIT1,
    E32_GPIO_BEC_BIT2, E32_GPIO_PARK, E32_GPIO_AUX,
    E32_GPIO_INPUT_LINE
} e32_am13e_gpio_role_t;

typedef enum {
    E32_RX_MODE_AUTO, E32_RX_MODE_SERVO, E32_RX_MODE_DSHOT,
    E32_RX_MODE_AUX_EDGE, E32_RX_MODE_SOFTWARE_SERIAL
} e32_am13e_rx_mode_t;

typedef enum {
    E32_LINK_UART_HALF_DUPLEX, E32_LINK_UART_FULL_DUPLEX,
    E32_LINK_SOFTWARE_UART
} e32_am13e_link_kind_t;

typedef enum {
    E32_SERIAL_PARITY_NONE, E32_SERIAL_PARITY_EVEN
} e32_am13e_parity_t;

typedef struct {
    e32_am13e_link_kind_t kind;
    uint32_t baud;
    e32_am13e_parity_t parity;
    uint8_t stop_bits;
    bool inverted_rx;
    bool inverted_tx;
} e32_am13e_serial_profile_t;

typedef enum {
    E32_LINK_ARM_RX, E32_LINK_SEND, E32_LINK_ABORT,
    E32_LINK_FLUSH_AND_REARM
} e32_am13e_link_action_t;

typedef enum {
    E32_TIMER_ARMING, E32_TIMER_AUDIO_NOTE,
    E32_TIMER_AUDIO_SAMPLE, E32_TIMER_SBUS_SLOT,
    E32_TIMER_SOFT_UART, E32_TIMER_PWM_BLANKING,
    E32_TIMER_RECEIVER_TIMEOUT
} e32_am13e_timer_role_t;

typedef enum {
    E32_EVENT_RX_DATA, E32_EVENT_RX_IDLE, E32_EVENT_TX_DONE,
    E32_EVENT_ERROR, E32_EVENT_TIMEOUT
} e32_am13e_link_event_t;

/* Existing shared Application -> AM13E native physical hardware. */
int      e32_am13e_bridge_stage(const e32_am13e_drive_plan_t *plan);                          /* A01 */
int      e32_am13e_bridge_set_pwm(uint32_t period_ticks, const uint32_t compare_ticks[3]);    /* A02 */
int      e32_am13e_bridge_idle(e32_am13e_idle_t mode);                                       /* A03 */
void     e32_am13e_bridge_force_off(void);                                                    /* A04 */
int      e32_am13e_bridge_commit(void);                                                       /* A05 */
void     e32_am13e_bridge_commutation_enable(bool enabled);                                  /* A06 */
int      e32_am13e_bemf_arm(uint8_t filter, uint32_t timeout_rel17_ticks);                   /* A07 */
int      e32_am13e_bemf_schedule_from_handler(uint32_t delay_rel17_ticks);                  /* A08 */
void     e32_am13e_bemf_disarm(void);                                                         /* A09 */
int      e32_am13e_tick_start(uint32_t rate_hz);                                            /* A10 */
void     e32_am13e_watchdog_start(void);                                                      /* A11 */
void     e32_am13e_watchdog_feed(void);                                                       /* A12 */
uint32_t e32_am13e_reset_cause(void);                                                         /* A13, Boot B06 */
void     e32_am13e_fatal_reset(void) __attribute__((noreturn));                               /* A14 */
int      e32_am13e_config_store(const void *cfg_ram, size_t len);                           /* A15 */
int      e32_am13e_bidir_transmit(const uint16_t *compare_words, uint32_t word_count,
                                     uint32_t reply_delay_rel17_ticks);                       /* A16 */
int      e32_am13e_gpio_write(e32_am13e_gpio_role_t role, uint8_t value);                   /* A17 */
int      e32_am13e_gpio_read(e32_am13e_gpio_role_t role);                                   /* A18 */
int      e32_am13e_hall_capture_arm(uint32_t timeout_rel17_ticks, bool enabled);            /* A19 */
int      e32_am13e_receiver_capture_mode(e32_am13e_rx_mode_t mode, uint32_t period_ticks);  /* A20 */
int      e32_am13e_receiver_link(e32_am13e_link_action_t action,
                                    const e32_am13e_serial_profile_t *profile,
                                    const uint8_t *tx, uint32_t tx_len);                     /* A21 */
int      e32_am13e_telemetry_configure(const e32_am13e_serial_profile_t *profile);          /* A22 */
int      e32_am13e_telemetry_transfer(e32_am13e_link_action_t action,
                                        const uint8_t *tx, uint32_t tx_len);                 /* A23 */
int      e32_am13e_timer_program(e32_am13e_timer_role_t role,
                                   uint32_t delay_rel17_ticks, bool enabled);               /* A24 */
uint32_t e32_am13e_unique_id_word0(void);                                                     /* A25 */

/* Native interrupt/event wrappers -> original shared Application logic. */
void     e32_app_on_commutation_due(void);                                                     /* C01 */
int      e32_app_on_bemf_capture(uint32_t elapsed_rel17_ticks);                               /* C02 */
void     e32_app_on_bemf_timeout(void);                                                        /* C03 */
bool     e32_app_on_dshot_capture(const uint16_t edge_intervals[32],
                                    uint16_t two_bit_ticks, bool inverted);                   /* C04 */
bool     e32_app_on_pwm_pulse(uint32_t high_us, uint32_t period_us);                           /* C05 */
void     e32_app_on_hall_event(uint32_t elapsed_rel17_ticks, bool timeout);                   /* C06 */
void     e32_app_on_receiver_edge(uint32_t capture_rel17_ticks, bool timeout,
                                    bool level_high);                                         /* C07 */
void     e32_app_on_receiver_link_event(e32_am13e_link_event_t event,
                                         const uint8_t *bytes, uint32_t length);               /* C08 */
void     e32_app_on_telemetry_link_event(e32_am13e_link_event_t event,
                                          const uint8_t *bytes, uint32_t length);              /* C09 */
void     e32_app_on_aux_pulse(uint32_t high_us, uint32_t period_us);                          /* C10 */
void     e32_app_on_aux_timer_event(e32_am13e_timer_role_t role);                             /* C11 */

/* Existing Boot protocol -> AM13E native Boot hardware. */
int      e32_am13e_boot_read_block(uint16_t logical_block, uint8_t *dst, uint16_t byte_len); /* B01 */
int      e32_am13e_boot_write_block(uint16_t logical_block,
                                     const uint8_t *src, uint16_t byte_len);                  /* B02 */
uint32_t e32_am13e_boot_device_id(void);                                                       /* B03 */
bool     e32_am13e_boot_application_valid(void);                                              /* B04 */
void     e32_am13e_boot_jump_to_application(void) __attribute__((noreturn));                  /* B05 */
/* B06: Boot has its own implementation of e32_am13e_reset_cause(), same A13 name/ABI. */
```

**C types above are proposed porting contracts.** The `e32_app_on_*` functions are thin AM13E-only dispatcher seams that invoke the original source handlers/algorithms; they do not redefine the decoders. A derived `drive_plan` contains only logical gate-state intent. Its physical application must use the board's validated driver enable/polarity/dead-time mapping, not interpret `STATIC_ACTIVE` as directly applying battery power or assume that complementary outputs are always enabled. Original per-target polarity variants are preserved through the mapping.

## 3. Mandatory common return, lifetime and arbitration conventions

**Success:** `int` native service returns `0` on failure and nonzero on accepted operation. Rejections leave a safe unchanged (or forced-off) output, never a partially applied bridge state. Read negative/positive values are not reused as Boolean status unless the declared API is explicitly a read. `e32_am13e_gpio_read()` returns `0/1`, negative on unavailable role.

**Atomic drive-plan group:** `A01` (modes), `A02` (three CCR-equivalent values), `A05` (commit) form one transaction; `A05` publishes a complete state only. Hardware break/trip must supersede all staged requests. `A04` aborts drive/deadlines, blocks future PWM until the original state machine explicitly starts again; no hidden motor rearm policy.

**Event generation:** `A06/A07/A08/A09/A19/A20/A24` change hardware event ownership. Disabling or reconfiguring an event invalidates previously armed interrupt callbacks. ISR wrappers sample and clear relevant hardware flags first, dispatch **at most once** for the current generation, and apply the source priority between event types. Do not deliver a stale capture after motor stop or receiver mode change. ISR callbacks execute in source-equivalent logical timing context; they never do an unbounded Flash operation.

**Units:** `_rel17_ticks` means a monotonic abstract time converted to the same mathematical epoch/resolution that the relevant rel17 source branch expects. It is **not** automatically AM13E peripheral-clock ticks. PWM `period_ticks`/`compare_ticks` represent values computed from original source formulas before native clock-domain conversion. Avoid lossy, ambiguous integer conversion; preserve the original filter/deadline/servo tolerances of enabled configurations.

**Feature and pin validity:** A board target must supply a satisfiable resource mapping for every feature it enables. Return failure for an unavailable role and treat that as a **target configuration/design error**; do not change the original state machine to silently bypass a source-selected feature. Mutually exclusive configurations remain separate upstream-style `add_target()` variants if needed.

## 4. Motor drive, BEMF, Hall and audio contracts

| ID | Behavior and preserved source reference |
|---|---|
| **A01** | Accept every `TIM1_CCMR*`/`CCER` logical combination generated by `nextstep()`, `laststep()`, `resetcom()`, brushed branch, `playmusic()` and `playsound()` for the selected `PWM_ENABLE`, `INVERTED_HIGH`, `cfg.damp` etc. Permissible plans include 3 independently PWM-driven sine phases, static forced outputs and non-six-step brushed patterns. Reject invalid shoot-through physical plans, not source-supported states. |
| **A02** | Preserve **per-channel** CCR1/2/3 compare values and source period/update rules (`FULL_DUTY`, DEAD_TIME, `TIM1_CR1` UDIS window). It must not duplicate one duty value into all three channels during sine/audio. Convert logical clock units using configured MCPWM timebase and record required saturation behavior as a board control parameter. |
| **A03** | Distinguish all-off, low-side short and source-selected brake. Do not collapse `resetcom()` or `cfg.duty_lock` semantics into emergency fault shutdown. |
| **A04** | Forced safe outputs + cancel all active drives/commutation deadlines for fault/reset. Hardware trip path must not wait for CPU software response. |
| **A05** | Publish staged phase modes/duty coherently, including a source-equivalent COM/update event, or defer until the designated period boundary exactly where source requires. Any transition to/from sine, brushed or music requires the same logical ordering. |
| **A06** | Own commutation IRQ enable/disable, generation/flag handling. `C01` may invoke the original step logic only when armed. |
| **A07** | Own CMPSS route/filter/timebase and BEMF timeout for rel17 `nextstep()`'s selected ERPM band **after** `compctl(pcc)` chooses phase. Filter/blanking configuration is not a new observer. |
| **A08** | Schedule source-computed phase deadline; reset timer reference at **accepted handler** for BEMF ISR, and at the rel17 requested update point for sine/start transitions. Do not shift reference silently to hardware capture timestamp. |
| **A09** | Disarm BEMF capture and previous deadline atomically, invalidate event generation and suppress queued callbacks. |
| **C01** | Deliver effective COM/due to source `tim1_com_isr()`/equivalent preserved logic exactly once. For legacy variants that clear a forced mode before `nextstep()`, reflect this in the source-derived native dispatch without changing the step algorithm. |
| **C02** | Implement source `iftim_isr()` logic using delivered elapsed capture time: early rejection (`t < ival/2`) leaves capture armed; accepted event updates `fast`, `ival`, `cfg.timing`, resets schedule, disarms capture and increments `sync`. Return `-1` for invalid native event, `0` rejected/too early, `1` accepted; this is *adapter reporting*, not an upstream state change. |
| **C03** | Match source timeout case: if both capture and enabled timeout are pending, **timeout is evaluated first**. Clear/invalidate capture, set source `sync=0`, `fast=0`, `ival=10000 << IFTIM_XRES`, `ertm=100000000`. |
| **A19/C06** | When `HALL_MAP` is compiled and selected by upstream runtime logic, configure Hall edge/reset timer, decode via existing `hallcode()` (including `USE_XOR` route where selected), deliver capture elapsed values and timeout to source `tim3_isr()` equivalent. Preserve phase resync and Hall/hybrid detection. No BEMF-only fallback hides a missing Hall backend. |
| **A24/C11** | Provide **source timer-role events** for 250-ms arming, AU audio sampling/note timing, SBUS2 telemetry slot, timer-bit-bang CLI and `SW_BLANKING` TIM1 update/compare. Timer role indicates which source handler receives the event; the MCU adapter never interprets a note, protocol byte, control state or motor command. |
| **Motor audio** | All source `playmusic()` note parsing, `playsound()` AU header/sample handling, `busy` arbitration and `delayf()` phase swap are retained. Use `A01/A02/A05` to realize output waveforms and `A24/C11` for timing; do not move audio melody/format decoding into HAL. |

## 5. Input transport, DShot, telemetry and auxiliary contracts

### DShot and Servo receiver ownership

- `A20/C07` transport the rel17 **edge/timeout events** to `entryirq()`/`calibirq()`/`dshotirq()`/`dshotresync()` equivalents. Native capture selects rising/falling timebases and DMA trigger behavior only; rel17 decides whether a signal is Servo/Oneshot, DShot or CLI and when to switch modes.
- `C05` supplies `high_us` and `period_us` to source `calibirq()`/`servoirq()` logic; preserve special **28–32 μs telemetry request**, `800–2200 μs` validity and calibration/watchdog policy from rel17. Return `true` only if the source accepted the observation, otherwise `false`; no new failsafe threshold is defined by HAL.
- `C04` provides the **32 input intervals**, DShot polarity and source-equivalent two-bit time to the unmodified CRC/command decoder. Preserve the exact raw edge sample order, original dshotarr1/2 scaling, command repetition, `dshotinv`, telemetry request and RX→TX pipeline. Return `true` for source-valid frame, `false` for timing/CRC/sync failure, so AM13E can execute the original `dshotresync()`/rearm transition. No PWM or UART mode priority is invented.
- `A16` accepts the **prepared source 23-word waveform buffer**, not a re-encoded GCR integer. `src/io.c` continues to own the GCR table, dshotval and prior-response preparation. The physical TX start in inverted mode precedes the current frame's final parsing in the original DMA handler; preserve that sequencing. `A16/A21` make RX capture and TX waveform ownership exclusive and restore RX on actual TX completion.
- `C10` delivers the `IO_AUX` captured pulse to upstream `setbrake()`; preserve source-specific Timer16/17 single-edge or alternate capture variant semantics.

### Serial receiver, CLI and telemetry

| ID(s) | Required native behavior |
|---|---|
| **A21/C08** | Preserve input `serialirq()`/`iofunc` receive dispatch: modes Serial, iBUS, SBUS/SBUS2, CRSF, EXBUS and HoTT; rel17 baud/parity/stop/inversion, DMA refill, half-duplex switch and idle/error/TC flags. `E32_LINK_SOFTWARE_UART` must emit/consume original start/data/stop bit event timing for the `!IO_PA2` CLI path. `C08` signals frame/timeout/TC/error with source data bytes; native transport does not parse frame checksum/commands. |
| **SBUS2** | Preserve `sbusirq1()` vs `sbusirq2()`, 125ns-like slot-timer abstraction and `telphid` slot selection, not just UART frame receive. Schedule via `A24/C11`; never transmit into an active RX-owned slot. |
| **A22/A23/C09** | Preserve `inittelem()` mode configuration, `usart1_isr()` idle/error/rearm handling, `usart1_tx_dma_isr()` queue refill and `sendtelemdata()` ownership. `A22` configures UART/soft physical profile. `A23` starts bounded DMA transmit, reports actual completion and releases line at proper point; `C09` never forwards uncommitted queue bytes. Protocol builders remain in `src/telem.c`. |
| **A17/A18** | Role-based native GPIO for `ERPM_PIN` toggle, `BEC_MAP` output selection, `PARK_PIN` input, optional `IO_AUX`/input line level. Preserve source debounce/state/polarity decisions; board config translates role to physical pin/pull/drive. `ledctl()`/`initled()` are already sufficient upstream Hooks for LED/WS2812, whose actual waveform is native. |
| **A25** | Expose genuine AM13E word of hardware identity corresponding to the source's `DESIG_UNIQUE_ID0` use in EXBUS responses. Preserve the original packet field width and protocol; do not fabricate STM32 identity data. |
| **`hsictl()`** | Provide meaningful native oscillator/timebase calibration response to source PWM period calibration in configurations enabling `cfg.throt_cal`. If the selected clock implementation has no matching trim mechanism, select a native method fulfilling the same calibration effect or record a hardware/platform incompatibility; do not silently make an enabled source Hook a no-op. |
| **`adctrig()`/`adcdata()`** | Preserve rel17 ADC argument positions and sensor/`SENS_CNT` guards, including `ANALOG_CHAN`, `TEMP_ADC2`, voltage/current/temperature/failsafe variables. Data sampling and physical scaling are MCU/board-specific; motor and protection equations remain in the source. |
| **`initgpio()`/`hallcode()`** | Source generic STM32 GPIO writes must be conditionally delegated to AM13E input/output setup without changing Hall code or parking/ERPM/BEC option semantics. The build must provide only one effective definition per Hook. |

**A20 mode and `A21` link are complementary:** A20 manages timer edges/DMA waveform capture; A21 manages serial/soft-UART physical transfer. Source input mode decides which is armed and `A24` provides exceptional interval timers. No DMA channel, physical pin or comparator can be double-owned simultaneously by two active modes; serialize transitions according to source `ioirq`/`iofunc` state.

## 6. Runtime, persistence, safety and reset contracts

| ID | Contract |
|---|---|
| **A10** | Start source-equivalent 16 kHz tick at rel17 initialization point, not unconditionally inside `init()`; PendSV and scheduler relations remain upstream. |
| **A11/A12** | Start/feed MCU watchdog with source-selected timeouts and entry/receiver policy; no kick on invalid receiver bytes that rel17 would reject. |
| **A13/B06** | Normalize POR, watchdog and software reset only as used by original arming/startup melody and Boot ACK code; clear native cause flags where original clears RCC_CSR. Boot and Application each link their own implementation. |
| **A14** | Reproduce source fatal sequence: motor gate-off (`A04`), visible fault indication if source enables it, original approximately 1 s wait and native reset trigger; never return. Safe exit to Boot uses a distinct original service-entry/reset sequence, not necessarily full fatal delay. |
| **A15** | Preserve `savecfg()`/`resetcfg()` results and `ertm`/`busy` preconditions. Store **only** the Cfg bytes from RAM into `CFG_BASE` 4KiB (not Reserved/App). Use 2 KiB sector RMW/verify, 16-byte aligned program steps, SRAM-executed command routine if current code bank conflicts. Do not reserialize Cfg into a new schema or alter `Cfg.id`. |
| **A24** | Timer role ownership for source arming/audio/SBUS/soft-UART/blanking must be explicit (sine and six-step commutation deadlines remain `A08`-owned): one hardware channel cannot be allocated to simultaneous incompatible roles. Unsupported board resource combination is a target planning defect, not an upstream feature deletion. |

## 7. Bootloader: original wire protocol and additive addressing

### 7.1 Original rel17 command set: IDs and semantics retained

```c
#define CMD_PROBE  0
#define CMD_INFO   1
#define CMD_READ   2
#define CMD_WRITE  3
#define CMD_UPDATE 4
#define CMD_SETWRP 5
#define CMD_WINDOW 6  /* AM13E Flash-layout extension; NOT upstream rel17 */
#define RES_OK     0
#define RES_ERROR  1
```

`recvval()` receives **one byte plus bitwise complement**, returns an integer or `-1` for timeout/data mismatch; `sendval()` uses the same two-byte form. `recvdata()` receives length as `recvval()` word count-minus-one, then `(count+1)*4` data bytes and CRC32; `senddata()` emits the same framing with original CRC32 polynomial/bit-order/complement. Data length is 4–1024 bytes, always word-aligned; no generalized new packet protocol is introduced.

| Command | Original entry/reply | Design contract |
|---|---|---|
| `PROBE` | No further payload; `RES_OK` | Native transport and original value framing; unchanged |
| `INFO` | 32-byte CRC-framed structure `REVISION`, `IO_PIN`, four MCU identity bytes, remaining zero | `B03` uses authentic AM13E identity; preserve offsets, width and total size. New Host recognizes target from existing `INFO` before sending ID 6. |
| `READ` | `num=recvval()`, `cnt=recvval()`, CRC-framed bytes `((cnt+1)<<2)` | `B01` reads 4–1024 bytes in one 1KiB logical block; Config-first translation below; no wrap, no out-of-region access. For an invalid range, **do not read Flash**: send `sendval(RES_ERROR)` instead of a data frame and stay in Boot programming mode. This response applies only to requests whose address was undefined/unsafe in upstream; valid original READ framing is unchanged. |
| `WRITE` | `num=recvval()`, `recvdata(buf)` (max 1024), then `RES_OK/RES_ERROR` | `B02` validates CRC via original `recvdata()`, then operates on Config/App logical block. **Return `RES_OK` only after erase/program/compare succeeds**. Reject out-of-range and protected sectors without touching Flash. |
| `UPDATE` | Receive up to Boot image length in 1KiB chunks into SRAM, `RES_OK` after each block; `update()` reboot on success, otherwise `RES_ERROR` | Keep entire stage-then-self-update behavior. Boot size changes from upstream 4 KiB to max16KiB: buffer sizing and loop count derive from `BOOT_END−BOOT_BASE`, and RAM Flash command code must not overwrite staged image. |
| `SETWRP` | `0x33` Off, `0x44` Boot, `0x55` Full; `setwrp(type)` reload/reboot, then fallback error only if it returns | Preserve three settings and effect after reset; no irreversible lock substituted for reversible Off. Hardware option/protection sequence must be performed with vendor-supported mechanism and safe reset. |
| `WINDOW` | New ID6; one complement-coded window `0` or `1`; one `RES_OK`/`RES_ERROR` result | Window starts at 0 on entry/reset, persists across READ/WRITE until changed. `effective_block=(window<<8)|num`; 0–491 valid; invalid window leaves previous state unchanged and returns `RES_ERROR`. Does not alter original READ/WRITE packet. |

**Host compatibility boundary:** Upstream Hosts using original `PROBE`/`INFO`/read/write packet shapes keep those shapes, but an old host assuming a 2 KiB CFG prefix or no extension **cannot** necessarily upload a complete image in the new layout. A Host must package exactly **4KiB Config prefix followed by actual Application bytes** (not a forced full 488KiB image) and use `WINDOW` before blocks ≥256. This exception is driven by Flash geometry/layout, not an additional firmware feature.

### 7.2 Flash logical address translation (`B01/B02`)

```text
Logical blocks n=0..3      → CFG_BASE + n*1024        (4 KiB Config prefix)
Logical blocks n=4..491    → APP_BASE + (n-4)*1024    (up to 488 KiB App)
Logical blocks n=492..511  → REJECT
Reserved [0x5000,0x6000)   → NEVER MAPPED BY READ/WRITE
```

For each request validate: `n` range; `len` range 4..1024 and divisible by 4; requested bytes entirely within one logical block; read/write mode and WRP state; destination physical address/bank; program alignment and physical sector boundary *before* unlock/erase. Perform sector RMW when source packet length is smaller than a 2KiB sector; preserve sibling 1KiB data. Preserve original `Cfg.id=0x32EA` at Config base and direct Application jump at `APP_BASE=0x6000`.

**Actual image size:** The 488KiB App allocation is an *upper bound*. Linker bounds, package contents and Host transfer length derive from the real compiled image; normal update does not pad/program an entire 488KiB partition by policy. Boot and Config are not overwritten by App-only updates unless the original chosen Host workflow intentionally includes Config blocks. This distinction is host packaging/layout control, not a new runtime feature.

### 7.3 Flash program, self-update, protection and launch

| Hook/service | Responsibility |
|---|---|
| **B01** | Bounded physical read of mapped logical Config/App blocks; never alias the Reserved/Boot region. Return success/failure; parser serializes original packet. |
| **B02** | Bounded Flash write with 2KiB erase-sector RMW and 16-byte program steps as required by supplied SDK implementation. Verify bytes before success, respect Boot/full WRP state and bank conflict. Does not parse CRC or Host commands. |
| **Existing `write()`** | Preserve original `(dst,src,len)` entry behavior and return; its AM13E path is a **native function implementation**. From `CMD_WRITE`, only B02-authorized Config/App destinations are valid. From the internally validated RAM-staged `update()` operation, Boot-region write is allowed through a separately guarded self-update path. Merely passing a Boot address must **not** grant `CMD_WRITE` the self-update privilege. No second public `boot_write` command. |
| **Existing `update()`** | Receive/stage Boot image in SRAM exactly as original, verify before committing, execute same-bank Boot Flash commands from SRAM, reset after successful update, return/fail as original on error. Ensure RAM staging and flash-command RAM code have nonoverlapping memory regions. |
| **Existing `setwrp()`** | Off allows writable Boot/Config/App; Boot mode protects `[0x0,0x4000)` while permitting Config/App; Full protects MAIN Flash. Persist/reset/reload per supported AM13E protection mechanism. The selected target configuration must retain reversible Off semantics. |
| **B03** | Provide true AM13E MCU identity, not a spoofed STM32 Device ID. Wire `INFO` fields keep original length/layout. |
| **B04** | Source-equivalent application eligibility begins with relocated `Cfg.id == 0x32EA`. Validate the minimum Cortex-M33 stack/reset vector requirements necessary to execute `APP_BASE`; do not invent a signed manifest, image rollback, FW selector or new Host status field. |
| **B05** | Terminate Boot peripheral/IRQ activity, set correct M33 vector context (including VTOR as required), MSP/reset entry and branch to Application; does not return. |
| **B06/A13** | Reset cause on entry and ACK behavior for source software/option-reset conditions. Reset flags normalized without manufacturing a different Boot application-valid condition. |

**Protection feasibility caveat (explicit, not silently closed):** The v1.6 design specifies three reversible logical effects, but supplied source/SDK documentation does **not independently prove** that every AM13E security/option policy can toggle persistently and reversibly across reload exactly like STM32 `setwrp()`. The **required** implementation is native and must be supported by an appropriate production NONMAIN/protection configuration. If not, the silicon/security-policy mismatch is a blocking incompatibility against the user's preservation requirement; the design does **not** claim a verified `SETWRP` implementation or authorize changing the command to fake `RES_OK`.

## 8. Native AM13E file organization, Hook ownership and build contract

The MCU file layout is **fixed to the existing ESCape32 directory convention**. `mcu/STM32G431/` is the Application naming reference; `boot/mcu/STM32G4/` is the Boot naming reference. These existing directories supply file roles, **not** a target register implementation to copy.

### Required original-name counterparts (planned, not yet created)

| New target file | Upstream filename reference | Contractual ownership |
|---|---|---|
| `mcu/AM13E/config.c` | `mcu/STM32G431/config.c` | Keep original MCU-side entry Hooks (`init()`, `compctl()`, `adctrig()`, `io_serial()`, `io_analog()`, and applicable LED/clock hooks); orchestrate optional low-level AM13E HAL services without rewriting source behavior. |
| `mcu/AM13E/config.h` | `mcu/STM32G431/config.h` | MCU-specific clock, signal-role and compile-time definitions and required declarations; no STM32 register API emulation. |
| `mcu/AM13E/config.cmake` | `mcu/STM32G431/config.cmake` | AM13E Cortex-M33 compile flags, TI SDK/CMSIS/DriverLib inputs and target-scoped build options. |
| `mcu/AM13E/config.ld` | `mcu/STM32G431/config.ld` | AM13E Application/RAM region and fixed `APP_BASE` symbols, with image-size upper bound and selected Flash-layout exception. |
| `mcu/AM13E/preset.c` | `mcu/STM32G431/preset.c` | Preserve the upstream `const uint16_t sinedata[]` symbol/data interface and resulting sine behavior. It is waveform data, **not** an invitation to redesign defaults or the control core. |
| `boot/mcu/AM13E/config.c` | `boot/mcu/STM32G4/config.c` | Retain Boot's original MCU `init()` and platform setup; no Motor HAL dependency. |
| `boot/mcu/AM13E/config.h` | `boot/mcu/STM32G4/config.h` | Boot-only MCU, transport/clock and Flash geometry definitions. |
| `boot/mcu/AM13E/config.cmake` | `boot/mcu/STM32G4/config.cmake` | Boot Cortex-M33 compile/link dependencies and SDK selection under native Boot target conventions. |
| `boot/mcu/AM13E/config.ld` | `boot/mcu/STM32G4/config.ld` | Independent Boot program/RAM layout, without inheriting STM32 flash origins or sizes. |

There is **no `boot/mcu/AM13E/preset.c`**, because the Boot reference has no such file. The names above are part of the platform-file organization; they do not increment the private interface count. Existing Hooks remain source-facing entry points, not renamed aliases for the same behavior.

### Private HAL implementation units under `hal/`

Both images have an independent, nested `hal/` directory. The list below is an **illustrative ownership split from the rel17 Mapping**, not a fixed module count or a new public API. Each `config.c` remains the source-visible MCU Hook entry and delegates hardware implementation as needed.

| Proposed AM13E-only internal file | Original source callers / Hooks | Interface ownership |
|---|---|---|
| `mcu/AM13E/hal/hal_motor.c` | `nextstep()`, `laststep()`, `resetcom()`, sound and brushed drive | `A01–A06`, original drive states and physical MCPWM |
| `mcu/AM13E/hal/hal_bemf.c` | `compctl()`, `iftim_isr()`, `tim1_com_isr()` | Existing `compctl()`, `A07–A09`, `C01–C03` |
| `mcu/AM13E/hal/hal_hall.c` | `getcode()/hallcode()/tim3_isr()` | `A18/A19`, existing `hallcode()`, `C06` |
| `mcu/AM13E/hal/hal_adc.c` | `adctrig()`, `adcdata()` and ADC/sensor DMA completion | Existing ADC Hooks and sensor callback units, preserving sampling/argument order |
| `mcu/AM13E/hal/hal_input.c` | `initio()`, DShot/PWM/CLI/AUX input ISR | `A16`, `A20/A21`, `C04/C05/C07/C08/C10` |
| `mcu/AM13E/hal/hal_serial.c` | Conditional serial receiver, half-duplex/parity and soft-UART transport | `A21`, `C08`; may be merged into `hal_input.c` without changing contracts |
| `mcu/AM13E/hal/hal_telem.c` | `inittelem()`, telemetry UART/DMA | `A22/A23`, `C09` |
| `mcu/AM13E/hal/hal_aux.c` | `initgpio()`, LED/WS2812, parking, BEC, ERPM | `A17/A18`, existing `initled()/ledctl()` |
| `mcu/AM13E/hal/hal_audio.c` | Motor sound/beep hardware waveform while the source parser and timing policy remain in ESCape32 | Drive-state services `A01–A06` and source sound callbacks; may be merged into `hal_motor.c` |
| `mcu/AM13E/hal/hal_system.c` | SysTick/PendSV, watchdog, fault/reset and clock trim | `A10–A14`, existing `hsictl()` |
| `mcu/AM13E/hal/hal_storage.c` | `savecfg()`, config image copy | `A15`, target-safe Flash operation |
| `mcu/AM13E/hal/hal_timing.c` | Sine/Hall/sound, SBUS, soft-UART and blanking timers | `A24`, `C11` |
| `boot/mcu/AM13E/hal/hal_flash.c` | Existing Boot `write()/update()` and `CMD_READ/WRITE` | Original Hooks, `B01/B02` |
| `boot/mcu/AM13E/hal/hal_protection.c` | Existing `setwrp()` | Original Hook, supported AM13E persistent protection sequence |
| `boot/mcu/AM13E/hal/hal_start.c` | `CMD_INFO`, original marker and Application jump | `B03–B05` |
| `boot/mcu/AM13E/hal/hal_uart.c` | Existing `initio()/recvbuf()/sendbuf()` | Original Boot transport Hooks |

The private modules may use internal `.h` declarations within their **own** `hal/` directory when needed. No common Application/Boot public HAL Header is mandated; do not introduce an independent `core/`, `platforms/` or additional firmware framework. Preserve all original ESCape32 source-level Hooks in the correct image's `config.c`. Do **not** move the motor state machine, input/telemetry protocol decoders or Boot command dispatch into HAL. Boot must never link the Application motor HAL, and Application must not link Boot HAL objects.

### Native build source-selection contract

The original root `CMakeLists.txt` defines `add_target(name mcu ...)`, whose `file(GLOB srcs src/*.c mcu/${mcu}/*.c)` scans **only the immediate directory**. Root Application targets invoke it from the repository root; Boot targets invoke the same function from `boot/CMakeLists.txt`. Consequently `hal/*.c` files are **not automatically compiled**.

Preserve the original `add_target()` interface and legacy target source discovery. For `mcu STREQUAL "AM13E"` only, add nested HAL sources and internal include path *after* the target is created:

```cmake
# Proposed AM13E-only addition inside existing add_target(); not implemented.
if(mcu STREQUAL "AM13E")
    file(GLOB am13e_hal_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/${mcu}/hal/*.c")
    target_sources(${elf} PRIVATE ${am13e_hal_sources})
    target_include_directories(${elf} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/${mcu}/hal")
endif()
```

This single target-conditional rule resolves the Application HAL at `mcu/AM13E/hal/` and the Boot HAL at `boot/mcu/AM13E/hal/`, because the invoking CMake source directory differs. Both `config.cmake` files separately define the correct CMSIS/TI SDK/DriverLib selection; root `FLASH_SIZE`, Startup/Vector, Linker, ABI and programmer assumptions also require **AM13E-only adjustments**. Keep one effective Startup/Vector definition per image and maintain existing ESCape32 Boot and Application target declarations and outputs. Creating `hal/` directories without adding this source-selection rule is not a complete build design.

No source files or CMake scripts are being changed by this design revision. The source-discovery code shown above is a planned integration patch, not proof of a successful target build.

## 9. Design-only acceptance check

The design is internally complete when:

1. Every rel17 feature-family row in [Integration_Mapping.md](Integration_Mapping.md) has a source-preserving action and named existing Hook or `A/C/B` service.
2. Every proposed private service above has a Mapping consumer, a prototype, units/event ownership, status and failure behavior; no private service invents an extra ESCape32 feature.
3. All original six Boot command wire/request/reply/functional paths are specified; `CMD_WINDOW` is additive and only addresses Flash layout.
4. Both linker/target views agree on region bounds, actual image-size limit, original `Cfg.id` gate, config-prefix mapping and Reserved exclusion.
5. All Application/Boot module and path names use `mcu/AM13E/` / `boot/mcu/AM13E/`; upstream STM32/AT32/GD32 build behavior remains unchanged.

**Outside this document's design phase:** compiling/modifying ESCape32 or the AM13E backend, Mock HAL runtime testing, PWM/BEMF bench timing, Flash power-cut tests, and motor-drive testing. Remaining physical pin/peripheral feasibility and the documented persistent-protection silicon compatibility dependency must be resolved for implementation, but must not be disguised as already tested or addressed by deleting an upstream feature.
