# AM13E23019 / ESCape32 Rel17 — Rev1.4 Source Interface Compliance

**Basis:** Original, unmodified
[Integration_Design.md](v1.4/Integration_Design.md),
[Integration_Mapping.md](v1.4/Integration_Mapping.md) and
[Interface_Contracts.md](v1.4/Interface_Contracts.md);
actual `am13e-port-v2` source and native ARM/Host gates.
**Result: PARTIAL.** This document is a source-level trace, **not**
a new API specification or hardware qualification.

The 26 proposed private A/C/B signatures are a design seam,
**not** a mandate for an independently implemented 26-function HAL.
Original ESCape32 hooks are first-choice entry points. Function names
below are actual present functions; a semantic mapping does **not**
prove electrical behavior or full feature parity.

## Source-to-interface map

| ID | Present native source / hook | Assessment |
| --- | --- | --- |
| A01 | `motor_safety.c::am13e_app_motor_sixstep_write`; `motor_aq_plan.c` | MAPPED; physical stage disconnected |
| A02 | `motor_safety.c::am13e_app_motor_pwm_apply` | MAPPED; hardware PWM pending |
| A03 | `motor_safety.c::am13e_app_motor_sixstep_idle`, drag/lock routines | MAPPED; real bridge truth table pending |
| A04 | `motor_safety.c::am13e_app_motor_fault_shutdown`, `motor_power_stage.c` | MAPPED; physical trip pending |
| A05 | `motor_safety.c::am13e_app_motor_commutation_commit` | MAPPED; shadow/update timing pending |
| A06 | `motor_safety.c::am13e_app_motor_commutation_enable` | MAPPED; external gate deliberately inactive |
| A07 | `motor_bemf.c::compctl`, `am13e_app_motor_bemf_interval_select` | MAPPED; comparator wiring pending |
| A08 | `motor_event_timer.c::am13e_app_motor_bemf_commutation_delay_us` | MAPPED; IRQ/timer epoch unqualified |
| A09 | `motor_bemf.c::am13e_app_motor_bemf_abort`, `motor_event_timer.c::am13e_app_motor_timing_cancel` | MAPPED; event race needs silicon |
| A10 | `system_runtime.c::am13e_app_motor_runtime_tick_init` | MAPPED; SysTick silicon timing pending |
| A11 | `input_watchdog.c::am13e_app_io_watchdog_prepare` | MAPPED; WWDT behavior pending |
| A12 | `input_watchdog.c::am13e_app_io_watchdog_feed` | MAPPED; WWDT behavior pending |
| A13 | `reset_cause.c::am13e_app_motor_reset_flags` | MAPPED; only source-defined reset classes |
| A14 | `src/main.c::hard_fault_handler` → `am13e_app_motor_fault_reset` | MAPPED; gate/fault recovery pending |
| A15 | `cfg_flash_runtime.c::am13e_app_cfg_commit`, original `savecfg/resetcfg` | MAPPED; power-loss pending |
| A16 | `command_reply.c::am13e_pb14_bidir_tx_start` | MAPPED; 30us physical turnaround pending |
| C01 | `motor_event_timer.c::TIMG12_0_IRQHandler` → `src/main.c::am13e_app_motor_on_commutation_event` | MAPPED to original `nextstep()` |
| C02 | `motor_bemf.c::ECAP1_IRQHandler` → `src/main.c::am13e_app_motor_on_bemf_event` | MAPPED; analog timing pending |
| C03 | BEMF timeout path → same callback with timeout flag | MAPPED; timeout precedence in source |
| C04 | `command_capture.c` / `command_decode.c` → `src/io.c::am13e_app_io_dshot_packet` | MAPPED for selected Reference mode |
| C05 | `src/io.c::am13e_app_io_servo_pulse`, `command_decode.c` | MAPPED; input-mode/timing pending |
| B01 | `boot/mcu/AM13E/flash_range.c::boot_am13e_read_range` | MAPPED; APP-only bounds |
| B02 | `boot/mcu/AM13E/flash_range.c::boot_am13e_write_range` + original `write()` | **Original Rel17 Boot write() restored**; Host CTest aliases name only |
| B03 | `boot/mcu/AM13E/device.c::boot_am13e_device_id` | MAPPED; original 32-byte response |
| B04 | `boot/mcu/AM13E/app.c::boot_am13e_application_valid` | MAPPED; Cfg.id=0x32EA + M33 APP vectors |
| B05 | `boot/mcu/AM13E/app.c::boot_am13e_launch_application` | MAPPED; real handoff pending silicon |
| B06 / A13 | `boot/mcu/AM13E/device.c::boot_am13e_take_reboot_ack` | Independently linked Boot reset-cause policy |

**Status clarification:** MAPPED refers only to an identifiable
function/semantic path; it does not mean the exact proposed
`e32_am13e_*` signature exists or that electrical acceptance passed.

## Native interface / naming gaps

| Rev1.4 requirement | Current observation | Disposition |
| --- | --- | --- |
| Keep original shared code, App + Boot directories | `src/`, `mcu/AM13E/`, `boot/src/`, `boot/mcu/AM13E/` with legacy targets retained | MATCHED |
| Reuse original `init/initio/compctl/adctrig/adcdata` | Current native implementation defines these entry hooks | MAPPED; conditional modes pending |
| Original Rel17 Boot `write()` | Production `boot/mcu/AM13E/flash.c` now exports `write`; parser calls it directly | FIXED; native Host only renames to avoid POSIX libc collision |
| Original Boot `update()` / `setwrp()` | AM13E handlers still return `RES_ERROR` | **MISSING — mandatory source feature gap** |
| Source-defined conditional input and telemetry | Analog receiver, serial receiver/telemetry, Hall and board-specific routes incomplete | PARTIAL — not excluded from v1.4 |
| Native `add_target()` with MCU-local `config.c/h/cmake/ld` | Application/Boot `config.cmake` own separate source selection; their `config.c` implement real `init()` hooks; linker still uses legacy-named files | PARTIAL — config.c/cmake aligned; config.ld pending |
| Native `entry.c` Startup bridge | Linked ARM vector/startup passes; standalone App/Boot `entry.c` absent | NEEDS actual startup ABI review, not automatically equivalent |
| Additional HAL / parallel control stack | MCU responsibilities in source modules; original Rel17 motor policy retained | No full independent HAL detected |

The current Reference Board isolates motor outputs and does not
activate unqualified Gate/OC/nFAULT/UART/current routes. This is a
**safe implementation state**, **not** permission to drop original
Rel17 conditional source capabilities.

## Next implementation gates

1. Finish App/Boot `config.ld` linker ownership
   under the existing ESCape32 `add_target()` without duplicate startup,
   resetting legacy targets, or inserting meaningless wrapper files.
2. Reuse original source hooks for selected conditional analog,
   serial, Hall and GPIO feature branches. Validate the actual board
   wiring before claiming any mode physically supported.
3. Implement `CMD_UPDATE` and `CMD_SETWRP` as original Rel17
   operations, after Bank0 SRAM execution/recovery and static NONMAIN
   reversible protection are qualified. Do not make unsupported
   hardware operations report success.
4. Confirm target on-silicon before claiming physical motor/protection
   qualification.

**Evidence:** Host/ARM CI, mock Flash + software tests and
static source mapping only; no hardware PASS.
