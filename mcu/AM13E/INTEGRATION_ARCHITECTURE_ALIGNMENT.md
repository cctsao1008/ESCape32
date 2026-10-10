> **Rev1.4 supersedes Rev1.1 as the design authority.**
> This document maps the former Rev1.1 proposal and is retained for history.
> Current v1.4 source/Boot parity must be checked against original ESCape32 rel17,
> not against this earlier interface inventory. The previous statement that
> 0x5000..0x5fff is FW2 configuration is obsolete; it is **Reserved**.
> The previous 50ms PWM/DShot auto-unlock is also obsolete.
> The v1.6 image CRC/signature contract is intentionally unchanged pending
> an explicit Boot ABI migration decision. CMD_UPDATE and CMD_SETWRP remain
> required but unimplemented on AM13E.
>
# ESCape32 Rel17 / AM13E23019 — Integration Architecture Alignment

**Review basis:** the user-supplied `Integration_Design.md`,
`Integration_Mapping.md` and `Interface_Contracts.md`, all **Rev1.1
(architecture-aligned, design-only)**. Comparison baseline:
`am13e-port-v2` (real executable source and host/ARM CI). The supplied
Rev1.1 documents intentionally did **not** claim firmware had been
implemented. This reconciliation records implemented semantic boundaries,
intentional project overrides and unresolved integration requirements.

## 1. Selected native architecture

```text
ESCape32/
  src/                 unchanged original Rel17 main/io/util/telem/prog
  mcu/AM13E/           native AM13E23019 MCU implementation; board providers,
                       tick, MCPWM0, ECAP0/1, ADC, config persistence
    flash_partition.h  shared 16KiB Boot, 4KiB Cfg, 4KiB Reserved, maximum 488KiB APP
  boot/src/            original ESCape32 Boot framing/commands + AM13E CMD_WINDOW
  boot/mcu/AM13E/      AM13E Boot transport, flash, image verification and launch
  future TI FW2        independent TI sensorless FOC source/build; NOT in FW1
```

The source directory name `AM13E` predates this review; it denotes the
same **AM13E23019 MCU backend** the design calls `AM13E23019`.
Avoid creating a second duplicate source tree just to rename the folder.
All existing STM32/AT32/GD32 target flows remain intact. Upstream Rel17
source entry hooks remain `initio()`, `compctl()`, `adctrig()`,
`adcdata()`, `savecfg()`, `resetcfg()`, `nextstep()` etc.

**No 26-function wrapper HAL is introduced.** The three design files
explicitly call their A/C/B signatures *proposed private interfaces*.
This table maps each **semantic contract** to implemented source/ABI.
Names need not be identical and no stub is added merely to satisfy a
diagram. The native source and exact linker/boot protocol are binding.

## 2. Axx, Cxx, Bxx contract-to-source mapping

| Contract | Current implementation / ownership | Qualification |
| --- | --- | --- |
| A01–A03 | `motor_safety.c`: `am13e_app_motor_sixstep_write`, `am13e_app_motor_pwm_apply`, `am13e_app_motor_sixstep_idle`, Drag/Lock Braking | Six-phase register **software** only; output pads isolated |
| A04–A06 | `am13e_app_motor_fault_shutdown`, `am13e_app_motor_commutation_commit`, `am13e_app_motor_commutation_enable` | Safe motor shutdown/internal timer; PB13 stays inactive |
| A07–A09 | `compctl`, `am13e_app_motor_bemf_interval_select`, `am13e_app_motor_bemf_commutation_delay_us`, `am13e_app_motor_bemf_abort`, `am13e_app_motor_timing_cancel` | Real CMPSS/ECAP1/TIMG12 path; silicon latency pending |
| A10 | `am13e_app_motor_runtime_tick_init` and `system_runtime.c` | 16kHz SysTick, ARM/Host evidence |
| A11–A12 | `am13e_app_io_watchdog_prepare/feed` in `input_watchdog.c` | Valid PB14 command only; LFCLK/reset physical test pending |
| A13–A14 | `am13e_app_motor_reset_flags`, `hard_fault_handler`, `am13e_app_motor_fault_reset` | Native reset semantics; fail-closed |
| A15 | `am13e_app_cfg_commit`, `cfg_flash_plan.c`, `cfg_flash_writer.c` | FW1-only 0x4000..0x4fff; no FW2 write |
| A16 | `command_reply.c`, `bidir_codec.c`, `bidir_timing.c` | PB14 shared-line BiDShot 30us software policy |
| C01 | `am13e_app_motor_on_commutation_event` → original `nextstep` | IRQ-ACK then shared algorithm |
| C02–C03 | `am13e_app_motor_on_bemf_event(capture_us,timeout)` | Early rejection, accepted delay, timeout priority |
| C04 | `am13e_pb14_decoder_pulse` → `am13e_app_io_dshot_packet` | Full frame/CRC; no independent parser |
| C05 | `am13e_app_io_servo_pulse` (Rel17 policy) | PWM mode locked independently of DShot |
| B01–B02 | `boot_am13e_read_range/write_range`, `boot_am13e_flash_write` | `CMD_WINDOW=6` block0..487; 2KiB SRAM RMW |
| B03 | `boot_am13e_device_id` | Native AM13E ID, no STM32 impersonation |
| B04–B05 | `boot_am13e_application_valid`, `boot_am13e_app_validity`, `boot_am13e_launch_application` | Rel17 Cfg.id=0x32EA + M33 APP vectors; no APP CRC |
| B06 | `boot_am13e_take_reboot_ack` and independent Boot reset backend | No reliance on STM32 RCC flags |

For actual function signatures and reviewed behavior, use those source
paths; the supplied design signatures are **not drop-in ABI typedefs**.

## 3. Flash / Boot contract (aligned implementation)

| Region | Bytes | Owner |
| --- | --- | --- |
| `0x00000000..0x00003fff` | 16 KiB | Common Boot |
| `0x00004000..0x00004fff` | 4 KiB | FW1 settings only |
| `0x00005000..0x00005fff` | 4 KiB | Reserved (no active writer) |
| `0x00006000..0x0007ffff` | 488 KiB | ONE installed FW1 or FW2 |

`flash_partition.h` is consumed by actual FW1 parameter planner,
Boot range checks and image validator. Linker assertions retain
`APP_BASE=0x6000`; on-flash M33 vectors at `+0x000`, no APP signature/header/CRC. Code follows linked vectors;
the only Boot marker is persistent Cfg.id=0x32EA.

Rel17 commands 0–5 retain original numeric identities. AM13E adds
`CMD_WINDOW=6`, complement-framed selector `0/1`, default 0 at
Boot entry. Effective block = `window*256 + legacy_byte`;
valid blocks 0..487, invalid 488..511 rejected before Flash access.
Existing 1KiB READ/WRITE payloads and CRC32 remain unchanged.
A **257 KiB** packed fixture produced from the real ARM-linked FW1
is transferred through the actual shared Boot command dispatcher and
compared byte-for-byte in Host CTest. No whole-image CRC or Signature-last commit exists; interrupted
writes may leave a bootable but incomplete APP.

The code/data writer preserves the nonwritten 1KiB half of each
2KiB erase sector using an aligned SRAM image; the first APP sector is an ordinary RMW sector, with no metadata
reservation or signature-last transition.
Boot and both parameter regions are outside its authorized range.

**Host compatibility warning:** installing an image larger than
256 KiB requires an updated WiFi-Link/updater that explicitly issues
`CMD_WINDOW=1`. No production host upgrade or on-target Flash
RMW endurance validation is claimed by this repository.

## 4. FW1 input and state contracts

- PB14 is the only application command/response wire: PWM RX,
  DShot150/300/600 RX and BiDShot TX. No dedicated UART telemetry
  path is activated.
- `command_decode.c` locks an accepted PWM or DShot receiver mode;
  mode selection persists across 50ms silence; decoder reset is
  required to choose another protocol.
  Neither a bad CRC nor truncated bit group feeds WWDT or throttle.
- Shared Rel17 DShot parser, comparator selection, BEMF timing and
  external `initio()`/service hooks are preserved.
- Motor Sixstep/Sine/Braking/Music/PCM retain shared MCPWM0 ownership.
  TIMG4 is exclusively PB14 TX; TIMG12 handles commutation or PCM.
- MCU physical input/output pinmux is **reference-board data**, not a
  universal fact of AM13E23019. A physical board design is not
  manufactured from this source arrangement.

## 5. Explicit scope overrides vs supplied Rev1.1

These are later user-authorized **product/porting scope decisions**;
they are not errors in the supplied design:

| Rev1.1 proposed/required behavior | Current binding decision |
| --- | --- |
| Enable Gate Driver via runtime bridge | **PB13 GPIO inactive IO-only**; no gate enable |
| Driver nFAULT hardware trip | **PB15 GPIO input IO-only**; no OST1, IRQ or polling |
| Independent OC fast shutdown | Optional pin config **IO-only**; no OST2 |
| Dedicated DC-bus current / Current Limit | Optional analog pin config **IO-only**; no current SOC, limit or fabricated current telemetry |
| Standalone Serial Telemetry TX | Optional Hi-Z GPIO config **IO-only**; no UART |
| Motor Music listed as not-selected optional feature | **Retained** per project requirement on the shared MCPWM0; never standalone GPIO buzzer |

Because of these explicit exclusions, software coverage PASS **must
not be described as physical motor-drive/safety specification compliance**.

## 6. Remaining design/acceptance boundaries

1. **WiFi-Link Host upgrade** for `CMD_WINDOW=6` >256KiB images,
   including version negotiation and failure/retry handling. Current
   Host CTest models the protocol but does not update a real host.
2. **Firmware Type / Version service wire encoding** for FW1 versus
   independent TI FW2; the supplied documents leave the exact
   service payload as Detailed Design. Existing Rel17 `info`
   exposes revision/target but is not a standardized dual-FW
   machine-readable identity.
3. **FW2 TI sensorless FOC binary**, its parameter layout and
   APP_BASE vector/startup compliance require separate TI delivery.
   Nothing in this FW1 port claims to implement TI FOC or
   runtime FW switching, A/B, rollback or DroneCAN.
4. **TI review and board hardware**: Flash 2KiB sector behavior,
   same-bank SRAM command execution/readback, protection granularity,
   CCM security/startup details, PWM output/deadtime/IO polarity,
   PB14 3.3/5V contention, analog CMPSS and ECAP/DMA IRQ budget.
5. **Receiver mode policy**: original persistent receiver mode supersedes the 50ms gap; a
   Detailed Design convention needing input/failsafe system tests,
   not an upstream-mandated time or physical validation result.
6. **Reference board independence**: concrete oscillator, analog
   routes, phase pinmux and APP linker remain Reference Profile
   providers. A second runnable board/image profile has not been
   manufactured.

Architecture map review rules: preserve native Rel17 hooks, report
source and ABI semantics rather than file line numbers, and mark
changes to these open design choices explicitly before declaring
full product architecture sign-off.
