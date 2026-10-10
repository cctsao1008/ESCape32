# ESCape32 Rel17 → AM13E23019: v1.4 Source Conformance Audit

**Design authority:** attached `Integration_Design.md`,
`Integration_Mapping.md`, `Interface_Contracts.md`, Revision **1.4**;
original ESCape32 rel17 Commit
`1d718c143380c3eb7e6581478990e1496632e5b8`.
**Code branch:** `am13e-port-v2`.

**A source branch preserved in legacy code is not evidence of an
AM13E-executable backend.** Software CI is also not silicon acceptance.
The v1.4 documents are design rules, not completed implementation proof.

## Original source features and actual AM13E disposition

| Source-backed capability | Native AM13E disposition | Pending qualification |
| --- | --- | --- |
| `src/prog.c` configuration and command service | **Unmodified Rel17 parser** | Runtime serial transport not selected |
| `src/main.c` sensorless six-step/BEMF | **Host-tested, ARM-linked** with MCPWM/CMPSS/ECAP1/TIMG12 | Physical gate outputs isolated; motor not spinning |
| Servo PWM | **Host-tested RX adapter**, Rel17 `setthrot()` | On-silicon receive timing |
| Oneshot125 | **Host-tested RX adapter**, 125ns→Rel17 timer-domain scaling | Source clock calibration/entry transition still pending |
| DShot300/600/1200 and command CRC | **Host-tested decoder** and Rel17 `src/io.c` packet handler | 1200's ~1.67µs ECAP0 two-bit IRQ budget may not be feasible without DMA |
| BiDShot and extended telemetry | **Host-tested** prepared reply before CRC, next frame prepared at TX DMA completion | Electrical bidirectional turnaround, scope waveforms |
| Motor Sine/Brushed/Brake/Music/PCM | **Host-tested internal MCPWM0 ownership** | Power pads physically disconnected |
| Analog input mode | **Conditional native adapter missing** | VBUS/NTC ADC monitoring is not analog receiver `input_mode=1` |
| Serial/iBUS/SBUS/SBUS2/CRSF/EXBUS/HoTT input | **Conditional native adapter missing** | Legacy `src/io.c` transport excluded under AM13E; pins/UART not assigned |
| KISS/iBUS/S.Port/CRSF/MSB/HoTT telemetry | **Conditional native transport missing** | Legacy `src/telem.c` formatters retained, Serial TX IO-only |
| Hall/hybrid commutation | **Conditional native Hall adapter missing** | Original `HALL_MAP` branches retained in non-AM13E source |
| BEC/LED/ERPM/PARK/Beacon | **Conditional board adapter missing** | Original guarded source remains; no fabricated pinout |
| Current sensing/limiting | **IO-only by project decision** | No source-level deletion or invented ADC current |
| `savecfg/resetcfg` persistence | **Host-tested FW1 config writer** | Flash wear/power interruption still untested |

`PWM_ENABLE` controls *motor output*, not servo input.
`SENS_MAP` omission is the original default, not proof of
physical current sensing. Neither the original conditional features
nor their compile guards may be deleted merely to make this table shorter.

## Boot original command parity

| ID | Command | Current AM13E | v1.4 requirement |
| ---: | --- | --- | --- |
| 0 | `CMD_PROBE` | Host-tested | Preserved |
| 1 | `CMD_INFO` | Host-tested, native ID and 32-byte CRC frame | Preserved |
| 2 | `CMD_READ` | Host-tested, APP-only | Preserved |
| 3 | `CMD_WRITE` | Host-tested, 1KiB logical / 2KiB SRAM RMW | Preserved, HW pending |
| 4 | `CMD_UPDATE` | **Returns RES_ERROR** | **MISSING mandatory self-update/reset** |
| 5 | `CMD_SETWRP` | **Returns RES_ERROR** | **MISSING mandatory reversible static WRP** |
| 6 | `CMD_WINDOW` | AM13E-specific, Host-tested up to 488KiB | Additive, not original |

**The six original Boot commands are NOT all complete.** In particular
command-ID recognition is not functional preservation. Self-update
requires bounded 16KiB SRAM image staging, SRAM-executing Bank0
erase/program/copy/verify/reset and brown-out testing. Persistent Off /
Boot-only / Full protection requires a reversible TI NONMAIN static
protection update/reload design. Runtime dynamic Flash protection
alone does not satisfy `setwrp()`; do not change silicon security
lifecycle or erase the Boot vector without qualified recovery.

## Partition and selected v1.4 Boot validity

| Region | Selected current v1.4 address |
| --- | --- |
| Boot | `0x00000–0x03FFF` |
| ESCape32 configuration | `0x04000–0x04FFF` |
| **Reserved, not FW2 config** | `0x05000–0x05FFF` |
| Single Application | `0x06000–0x7FFFF` |

The approved and **now active** `AM13E_IMAGE_PROFILE=REL17_V14`
build uses `Cfg.id=0x32EA` at the beginning of the `0x4000`
configuration region and the M33 initial MSP/Thumb Reset PC at
`APP_BASE=0x6000`. The active `boot/mcu/AM13E/app.c` and
`app_validity.c` no longer validate an APP+0x400 signature, APP+0x500
header, or embedded image CRC. Those are superseded v1.6 ABI artifacts,
not v1.4 Boot acceptance rules.

The active `linker_app_rel17.ld` places vectors first, followed by
actual linked code and data, with an upper bound of **488 KiB** for
`[0x6000,0x80000)`. `AM13E_FW1_REL17.bin` contains actual linked
firmware bytes, not a fixed-capacity image; the flat transport image
adds only 0–3 `0xff` bytes for 4-byte wire alignment. The external
JSON sidecar is informational, not read by Boot. `CMD_WINDOW=6`
makes all 488 addressable blocks reachable by an upgraded host.

**Integrity limitation:** removing Signature-last / whole-image CRC
also removes the former torn-update protection. If the `Cfg.id`
and vectors remain valid but later code is incomplete, original
Rel17 Boot validity can still succeed. Command-frame CRC validates
each individual received WRITE, not overall firmware completeness.
No power-loss-safe update claim is made without a separately qualified
end-to-end update/recovery procedure. Existing v1.6 packaged updates
have no implicit migration or compatibility guarantee.

## Explicit five-feature IO-only scope

PB13 Gate Enable inactive; PB15 nFAULT input without fault IRQ/OST;
Independent OC input-only; Serial TX Hi-Z input-only; Current Limit
analog-pin configuration only. No board Gate Enable/Trip/UART/current
limiting is silently activated. This later project-specific scope
remains in effect pending an explicit contrary instruction.

## Evidence classification

Previous CI `verify_rel17_coverage.py` establishes only the 13
selected FW1 software groups, 5 IO-only exclusions, ARM strict link
and Host regression coverage. The separate v1.4 contract audit does
not convert conditional original features into implemented native
adapters. Native peripheral timing, TI NONMAIN reversible programming,
Boot self-update, input calibration, actual encoder telemetry
and complete hardware compatibility remain outstanding.

**Conformance status: PARTIAL — source-aligned improvements in place;
Boot operations and multiple conditional MCU adapters missing;
the former v1.6 Image ABI has been replaced, while Boot operations and
conditional native adapters remain missing.**

## Documentation / Specification Synchronization

The active host guide is
[`boot/tests/am13e_host/README.md`](../../boot/tests/am13e_host/README.md).
Previous v1.6/v2 APP-header/CRC/Signature-last test instructions are
preserved only in
[`README_V16_SUPERSEDED.md`](../../boot/tests/am13e_host/README_V16_SUPERSEDED.md)
and explicitly **SUPERSEDED**. Historical porting and pseudocode ledgers
also carry a non-normative header. Obsolete `image_integrity.{c,h}`,
`verify_v16_image.py` and `image_reference_v16.cmake` are retained
as historical artifacts, **not** enabled in the Rel17 v1.4 Boot/FW1
build or Host CTest. Legacy-named linker/packer compatibility sources
must be judged by their current code, not their filenames.

The CI `verify_v14_docs.py` gate guards current host instructions,
partition/linked-size terminology, five IO-only boundaries, retired
image-ABI quarantine and continuing mandatory `CMD_UPDATE` /
`CMD_SETWRP` plus conditional-backend gaps. Its PASS proves
documentation/source-reference consistency **only**, not implementation
of the missing operations or AM13E hardware acceptance.
