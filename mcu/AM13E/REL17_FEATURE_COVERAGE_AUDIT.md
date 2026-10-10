# AM13E23019 — Rel17 Software Evidence and Rev1.4 Open Gaps

**Design authority:** the three supplied ESCape32 Integration Design
Rev1.4 files, indexed at [V14_DESIGN_AUTHORITY.md](V14_DESIGN_AUTHORITY.md).
**Feature authority:** original ESCape32 Rel17 `src/`, `src/defs.h`,
`README.md`, target CMake and native MCU hooks.
This document records **evidence and gaps**, not additional requirements.

## What Native CI proves

| Evidence | Supported interpretation |
| --- | --- |
| Complete Rel17-derived ARM FW1 strict ELF link | Real source/backend is linked with the selected AM13E reference providers |
| 16 KiB Boot ELF / linked flat APP BIN | Actual linked-size file; maximum APP allocation is 488 KiB, not fixed image length |
| Native Host regressions | Software policy/encoding, 1 KiB wire blocks / 2 KiB Flash RMW, negative paths |
| CMD_WINDOW=6 tests | Original commands plus additive window-0/1 addressing, last valid block 487 |
| Source audit | Original Rel17 branches still exist; unimplemented AM13E adapters are explicitly identified |
| Reference electrical state | Gate/phase pads currently disconnected, nFAULT/OC/UART/current routes incomplete; **not a product exclusion** |

None of these constitutes Silicon Flash/Interrupt/DShot timing,
power stage safety, live motor, on-target Boot self-update or hardware
write-protection qualification.

## Must preserve per Integration Design Rev1.4

Source-defined and conditional: Servo/Oneshot125, DShot300/600/1200,
BiDShot / extended telemetry, analog and serial receiver modes,
iBUS/SBUS/SBUS2/CRSF/EXBUS/HoTT inputs, KISS/iBUS/S.Port/CRSF/MSB/HoTT
telemetry, Six-step/BEMF, Sine, Brushed, Hall/hybrid, braking, motor
audio, conditional GPIO/BEC/LED/PARK/ERPM, voltage/current/temperature,
config persistence, service commands and watchdog semantics.

The selected board need not enable every mutually exclusive source
branch simultaneously. An unselected conditional adapter still remains
in **Porting Gap**, rather than being deleted or declared out of scope.
Physical pins/sense/polarity are board-owned; no invented pin assignment.

## Interface / HAL / naming boundaries

- Retain native `add_target(name mcu ...)` in Application and Boot,
  target directories `mcu/AM13E/` and `boot/mcu/AM13E/`, plus
  upstream STM32/AT32/GD32 definitions.
- Preserve original `init()`, `initio()`, `compctl()`, `adctrig()`,
  `adcdata()`, `savecfg()`, `resetcfg()`, Boot `write()`,
  `update()`, `setwrp()` interface semantics.
- The 26 named v1.4 A/C/B private functions are **proposed design
  signatures**, not an obligatory 26-wrapper HAL or a license to
  introduce a 99-API framework. Use source-semantic mapping.
- No artificial regex ban on descriptive C/H filenames. Filenames,
  mapping and unit boundaries follow the native ESCape32 structure,
  with target-dependent code owned by the AM13E MCU directories.

## Blocking work (PARTIAL)

1. Boot `CMD_UPDATE` self-update and `CMD_SETWRP` persistent reversible
   protection currently return `RES_ERROR`; both are **mandatory,
   unimplemented** source capabilities, not approved omissions.
2. Analog/serial receivers, corresponding UART telemetry, Hall/hybrid
   and other conditional board I/O still lack native qualified adapters.
3. Reference motor power pads are not enabled and current/fault routing
   is unqualified. This is safe hardware gating, not functional parity.
4. Real host `CMD_WINDOW` integration, Flash power-fail, recovery,
   physical MCPWM/BEMF/DShot and motor-drive validation remain pending.

**Status:** HOST/ARM SOFTWARE CI EVIDENCE ONLY; REV1.4 FUNCTIONAL PARITY
INCOMPLETE; HARDWARE VALIDATION PENDING.
