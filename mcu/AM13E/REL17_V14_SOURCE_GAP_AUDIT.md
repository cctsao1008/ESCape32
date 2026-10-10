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

## Partition and image validity conflict

| Region | Selected current v1.4 address |
| --- | --- |
| Boot | `0x00000–0x03FFF` |
| ESCape32 configuration | `0x04000–0x04FFF` |
| **Reserved, not FW2 config** | `0x05000–0x05FFF` |
| Single Application | `0x06000–0x7FFFF` |

The common `flash_partition.h` and APP Linker enforce Reserved.
However, the **current v1.6 image contract** requires an extra marker
at APP+0x400, a header at APP+0x500 and whole-image CRC with
signature-last commit. **v1.4 instead requires the upstream `Cfg.id=0x32EA`
at CFG+0 and a relocated M33 vector at APP_BASE**, without making a
new image manifest mandatory. These are mutually different launch
contracts. Existing ABI remains untouched until an explicit user
choice and update/recovery migration design; therefore this difference
is an **OPEN ARCHITECTURE CONFLICT**, not a compliance PASS.

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
existing v1.6 Image ABI conflicts with v1.4 source-equivalent validity.**
