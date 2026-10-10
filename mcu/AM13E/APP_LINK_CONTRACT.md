# AM13E23019 — ESCape32 Rel17 Rev1.4 Boot / Application Contract

**Status:** native Rel17 v1.4 Boot validity selected and implemented; ARM
strict link / native Host CI validated. No on-silicon qualification or
production release approval.

This **supersedes** the former v1.6 requirement for APP+0x400
signature, APP+0x500 image header, full-image CRC and Signature-last
commit. They are **not active Boot/Image ABI requirements**.

## Memory map

| Physical Flash | Size | Owner |
| --- | --- | --- |
| `0x00000..0x03fff` | 16 KiB | Common Boot |
| `0x04000..0x04fff` | 4 KiB | ESCape32 Cfg; `Cfg.id=0x32EA` begins at `0x4000` |
| `0x05000..0x05fff` | 4 KiB | Reserved; no active writer |
| `0x06000..0x7ffff` | **488 KiB maximum** | Exactly one Application |

`APP_BASE=0x6000`; APP physical region is `[0x6000,0x80000)`.
**Actual firmware size is determined by the linked Application binary
and must not exceed the allocated 488 KiB region.** Nothing requires
a 488 KiB output for a smaller program.

Boot accepts Application launch only when the first halfword of
persistent Cfg equals `0x32EA` and the M33 initial MSP/Thumb Reset
PC from `APP+0x000/0x004` are plausible. MSP alignment, RAM_S bounds,
Thumb bit and APP physical PC bounds are checked. VTOR is set to
`0x6000` before launch. There is **no** Boot-accessible linked-image
length field, whole-image CRC or additional image signature.

## Build and delivery

| Target / file | Role |
| --- | --- |
| `AM13E_FW1_REL17.elf` | Complete ARM application, `mcu/AM13E/linker_app_rel17.ld` |
| `AM13E_FW1_REL17.bin` | Real `arm-none-eabi-objcopy` linked binary |
| `AM13E_FW1_REL17.flat.bin` | Identical application bytes plus 0–3 erased `0xFF` bytes for 4-byte transport alignment |
| `AM13E_FW1_REL17.json` | External descriptive sidecar: actual image length and SHA256; **Boot does not parse it** |
| `BOOT5_PB14.elf` | 16 KiB Bootloader, common Rel17 parser plus TI Backend |
| `AM13E_APP_SMOKE` | Linker/packer test fixture only, not motor firmware |

The linked-image profile is `AM13E_IMAGE_PROFILE=REL17_V14`.
Unknown profiles are rejected. For object-only work, `NONE` is
allowed when `AM13E_ENABLE_FW1_REL17_IMAGE=OFF`. The superseded
`REFERENCE_V16` profile must not be treated as supported.

Native source of truth:
`boot/mcu/AM13E/app_validity.c`,
`boot/mcu/AM13E/app.c`,
`mcu/AM13E/linker_app_rel17.ld`,
`boot/tools/pack_am13e_rel17.py`,
`mcu/AM13E/tools/verify_rel17_image.py`.

## Boot wire transport

Original numeric commands remain:
`CMD_PROBE=0`, `CMD_INFO=1`, `CMD_READ=2`,
`CMD_WRITE=3`, `CMD_UPDATE=4`, `CMD_SETWRP=5`.
AM13E adds `CMD_WINDOW=6` to select a 256-block address window.
`effective_block = 256*window + block`; valid APP blocks are
0–487. Block 488 is rejected. Reset starts in window 0; firmware
over 256 KiB requires a matching host/updater implementation
of `CMD_WINDOW=1`. No production updater acceptance is asserted.

`CMD_WRITE` retains complement framing, per-command CRC and 1 KiB
logical blocks, using TI 2 KiB erased-sector SRAM read-modify-write
and byte verification. A short final block is allowed. Boot, Cfg
and Reserved ranges are not writable by APP commands.
There is no fixed full-image transfer length, no APP CRC
and no Signature-last finalization sequence.

`CMD_UPDATE` (Boot self-update) and `CMD_SETWRP` (persistent
reversible write-protection modes) currently **return RES_ERROR**.
They remain **mandatory original functionality gaps**, not
accepted implementations. The Boot self-update safety prerequisites
and unchanged fail-closed behavior are tracked in
[CMD_UPDATE_SOURCE_GAP_REVIEW.md](CMD_UPDATE_SOURCE_GAP_REVIEW.md).

## Failures and qualification boundary

Native Rel17 `Cfg.id` plus M33 vectors cannot prove completeness
of later application code. A torn/incomplete firmware update may
leave valid Cfg/vectors and still be booted; the original wire-frame
CRC does not provide whole-image commit or atomic update safety.
An external update, verification and recovery procedure requires
hardware and host qualification. The superseded v1.6 packed format
is **not silently migratable** to this flat binary contract.

Evidence is limited to ARM ELF/linker/objcopy and host-test models;
Flash same-bank behavior, power-fail recovery, transport electrical
timing and real motor operation still need silicon testing.
The Reference image still has physically disconnected or unqualified
Gate/OC/nFAULT/UART/current routes. That is an **observed unqualified
implementation state**, not a Rev1.4 exemption from upstream conditional
feature preservation. See [V14_DESIGN_AUTHORITY.md](V14_DESIGN_AUTHORITY.md).
