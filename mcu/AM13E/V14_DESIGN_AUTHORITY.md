# AM13E23019 / ESCape32 Rel17 — Rev1.4 Design Authority

**Status:** authority index for the original user-provided Rev1.4
design package. This index introduces **no independent architecture
requirements**. The original three documents govern; original ESCape32
Rel17 source governs function availability/behavior. ARM/Host CI
coverage is implementation evidence, NOT hardware acceptance.

## Canonical source provenance

Uploaded archive: `ESCape32_AM13E_Integration_Design_v1.4(2).zip`.
The **verbatim** original Rev1.4 files are now tracked under
[`mcu/AM13E/v1.4/`](v1.4/Integration_Design.md), and CI checks
their SHA-256. They are the design authority; downstream audits are
implementation evidence, not replacement requirements.

- [Integration_Design.md](v1.4/Integration_Design.md)
- [Integration_Mapping.md](v1.4/Integration_Mapping.md)
- [Interface_Contracts.md](v1.4/Interface_Contracts.md)

Original names and hashes:

| Original file | SHA-256 |
| --- | --- |
| `Integration_Design.md` | `06acbc4960e09d5c2ebe266e0961a5a1847a0748cd3a33f544865fb5040943f5` |
| `Integration_Mapping.md` | `ad833a9182d7766d59ce51c92a22c8efadff5dde581b16f7cb20eee3b37d2039` |
| `Interface_Contracts.md` | `c7d8d2744806d8bf138d721a9b6f373661b780be5381c8eac8038132f4c4743c` |
| Original uploaded ZIP | `f3d31a7d5c1c6a1d541b0e9880b156c5cd65d631c2eecee59fce6e9537a682bc` |

**Precedence:** (1) ESCape32 Rel17 upstream source, `README.md`,
`src/defs.h`, native CMake and MCU hooks for features and behavior;
(2) the three original Integration Design Rev1.4 files for target
integration / proposed seams / partition; (3) TI SDK/TRM for AM13E
hardware facts; (4) implementation-specific docs as **evidence**, never
as a new exclusion or conflicting policy. Historical rules have no
authority and are removed from this working branch.

## Rev1.4 interpretation (not a replacement for the original files)

- **Native ESCape32 extension**, one Rel17 Application and original
  ESCape32-derived Bootloader; no separate HAL framework or 99-API
  abstraction. Retain `add_target(name mcu ...)` for both targets.
- The source's existing `init()`, `initio()`, `compctl()`,
  `adctrig()`, `adcdata()`, `savecfg()`, `resetcfg()`,
  Boot `write()`, `update()`, `setwrp()` take precedence over
  invented wrappers. A/C/B signatures in `Interface_Contracts.md`
  are **26 proposed private function names**, not verified exported
  symbols or a compulsory replacement HAL. `B06` reuses `A13`.
- `mcu/AM13E/` owns Application hardware differences;
  `boot/mcu/AM13E/` owns Boot hardware differences. Original
  STM32/AT32/GD32 targets remain untouched. No ad hoc source-filename
  blacklist or extra MCU selector convention is imposed.
- **Preserve all upstream source-defined/conditional functionality**;
  board-inapplicable features stay conditional and pending adaptation,
  not deleted or permanently excluded because of an earlier reference
  configuration. In particular PB14-only and five universal IO-only
  exemptions are **not Rev1.4 obligations**.
- Preserve all original Boot commands **0..5**, including full
  `CMD_UPDATE` and `CMD_SETWRP` behavior; add only
  `CMD_WINDOW=6` for the required address extension. A command that
  returns `RES_ERROR` in place of its functionality remains
  **unimplemented**, not compliant.
- Rev1.4 APP validity: persistent `Cfg.id=0x32EA` at `0x4000`
  plus native M33 vectors at `APP_BASE=0x6000`. Original per-command
  CRC remains; no v1.6 image header, whole-image CRC or
  Signature-last launch condition.
- Flash: 16 KiB Boot, 4 KiB Cfg, 4 KiB Reserved, up to **488 KiB
  allocated APP region**. Actual image size is its *linked binary*
  length. No A/B images, rollback or invented firmware services.
- Unqualified physical outputs remain safe/disconnected pending actual
  schematic, MCU peripheral and silicon validation; this **does not
  waive** rel17 conditional feature preservation.

## Current implementation status is not a policy

See [REL17_V14_SOURCE_GAP_AUDIT.md](REL17_V14_SOURCE_GAP_AUDIT.md),
[REL17_FEATURE_COVERAGE_AUDIT.md](REL17_FEATURE_COVERAGE_AUDIT.md)
and [REFERENCE_BOARD_STATUS.md](REFERENCE_BOARD_STATUS.md).
These report gaps in current native AM13E implementation, not
exemptions from original rel17 or Rev1.4. Only physical verification
can close physical qualifications; a green Host CI cannot.
