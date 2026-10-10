# E62 AM13E23019 — FW1 v1.6 Boot / Application Link Contract

**Status: Software port integrated and CI/Host verified; physical HW validation
and TI review remain outstanding.** Latest evidence:
[Run #38059589632](https://github.com/cctsao1008/ESCape32/actions/runs/38059304224).

## Authoritative rules

The E62 Software Architecture Baseline **v1.6** specifies:
- Boot: `0x00000000..0x00003FFF` (16 KiB).
- FW1 config: `0x00004000..0x00004FFF` (4 KiB), preserved on app update.
- FW2 config: `0x00005000..0x00005FFF` (4 KiB), preserved on app update.
- **One** FW1 or FW2 installed in `0x00006000..0x0007FFFF` (488 KiB).
- Exactly one boot-to-application entry/vector base at `0x00006000`.
- No A/B, bank swap, rollback, or runtime FW selection.

Those addresses are architectural. **Signature and metadata offsets below
are E62 detailed design**, not literal requirements from the v1.6 baseline.

## Unified v1.6 image format

| Element | Address | Mechanism |
|---|---|---|
| Cortex-M33 MSP/Reset vector table | `0x6000` | Actual TI GCC startup `.intvecs` |
| Signature ECC16 unit | `0x6400` | `0x32EA`, written LAST after verification |
| CRC image header (32 bytes) | `0x6500` | Target/length/payload CRC/header CRC |
| FW1 code and loadable ROM | `0x6800` onward | ARM ELF `.text`, load images |
| Mutable `.cfg` | SRAM_S `0x20000000..` | Persistent source `_cfg=0x4000` |
| Flash command `.TI.ramfunc` | SRAM_C `0x00C18000..` | Flash LMA + Startup copy |

The first **2 KiB physical Flash sector** contains vectors and metadata;
the deferred signature still prevents launch after a torn transaction.
Boot's `boot_am13e_application_valid()` checks signature, target/length,
CRC, MSP/Reset entry and image bounds; `boot_am13e_launch_application()`
sets VTOR to **0x6000** and transfers MSP/PC directly to the app.

Use the same single-image layout for a future FW2 implementation.

## Firmware build targets

- `AM13E`: original Rel17 + complete AM13E Backend OBJECT library.
- `AM13E_FW1_V16.elf`: **actual FW1 application ELF**, linker:
  `mcu/AM13E/linker_app_v16.ld`.
- `AM13E_FW1_V16_IMAGE`: objcopy BIN + CRC/metadata packed image + manifest.
- `BOOT5_PB14.elf`: actual common Bootloader, linker:
  `boot/mcu/AM13E/linker_boot_reference.ld` explicitly selected.
- `AM13E_FW1.elf` / `mcu/AM13E/linker_app_reference.ld`:
  **historical diagnostic ONLY**; not a v1.6 application artifact.

Image packer: `boot/tools/pack_am13e_v2.py` (its legacy file name is
retained for tooling continuity; code/manifest implement v1.6).

## Transport and update contract

The Bootloader retains the original 1 KiB, 8-bit READ/WRITE frames,
and adds the *AM13E-only* complement-framed `CMD_WINDOW=6` extension
from Integration Design Rev1.1. Window 0 is selected at Boot entry;
`CMD_WINDOW=1` addresses effective blocks 256..487 and rejects 488..511.
The full **488 KiB** application partition is now addressable; existing
hosts remain compatible for their original `<256 KiB` blocks but **cannot
upload larger images without a corresponding WiFi-Link host update**.

The physical writer snapshots an entire 2 KiB sector into aligned SRAM,
merges each 1 KiB logical block, then erases/programs/verifies the sector
through SRAM-resident Flash command functions. The other sector half is
preserved. Normal writes authorize APP only, never Boot or FW1/FW2 config.

Programming order remains `invalidate block 0, invalidate block 1,
code/data blocks >=2, restore block 0, restore block 1`.
Boot stages block 1's APP+0x400 signature ECC16 in RAM; it verifies
the full image and received span against the CRC/header before writing
that unit as the final committed data. Interrupted or stale updates
stay invalid until a new complete transaction.

## Evidence — 2026-10-10

From linked ARM GNU objects and Image Packer/Host CI:
- BOOT5_PB14.elf: **4,496 bytes** text+data+bss (not an on-chip test).
- AM13E_FW1_V16.elf: text **31,672**, initialized data **552**,
  BSS **5,336** bytes.
- ARM objcopy raw BIN: **33,968 bytes**; verified packed image:
  **33,968 bytes**, beneath the now-enforced 488 KiB APP transport bound
  (this historical payload size does not prove a large on-chip update).
- FW1 ELF symbols: `__app_vector_start__=0x6000`,
  `_cfg=0x4000`, `__ramfunct_start__=0x00C18000`.
- Boot ELF symbols: `__app_flash_start__=__app_vector_start__=0x6000`;
  real `boot_am13e_image_check` and `boot_am13e_launch_application`
  linked.
- Both ARM image-smoke and actual FW1 packed-image Host suites:
  **6/6 PASS each** (protocol, integrity, CRC, duplicate write,
  interrupted write, signature-last, simulated reboot).
- Real linked-image audit: **16 address, RAMFUNC, CRC, MSP/PC and
  manifest checks PASS** via `tools/verify_v16_image.py`.
- Existing Motor/BEMF/Audio/Flash FW1 Host/ARM GNU compile gates PASS.
- CMD_WINDOW=6 Host test covers 256/487 valid and 488 invalid.
- Dedicated 257 KiB real-ARM-prefix image fixture runs the actual
  Boot protocol, dual-window write, deferred signature and CRC check.
- 2 KiB SRAM-buffered RMW Host test preserves the adjacent 1 KiB half.

**Hardware and service limits:** there is no proven production WiFi-Link
implementation of CMD_WINDOW yet. Actual same-bank Flash execution,
power-interruption endurance and protection granularity require MCU
validation. Neither FW2 nor automatic Firmware Type/Version service
wire encoding is supplied by this FW1 port.

**Not established:** AM13E silicon execution, real Flash endurance or
reset/brownout behavior, actual PWM/BEMF/Audio function, gate/OC electrical
polarity and thresholds, production release approval. Those remain in the
separately scheduled final HW validation phase; they do **not** replace
any firmware Source Porting task.
