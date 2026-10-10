# AM13E23019 — Boot / FW1 Integration Status (Rel17 Rev1.4)

**Selected:** original ESCape32 `Cfg.id=0x32EA` at 0x4000 plus
plausible M33 vectors at `APP_BASE=0x6000`. The earlier v1.6
APP signature/header/CRC and Signature-last requirements are
**retired**. This is a software/CI milestone, not physical approval.

| Contract | Implemented evidence | Limit |
| --- | --- | --- |
| 16 KiB Boot `0x0000..0x3fff` | Linked `BOOT5_PB14.elf` | On-silicon Boot test pending |
| Cfg `0x4000..0x4fff` | Rel17 first halfword `0x32EA` required to launch | Cfg write endurance/power-loss pending |
| Reserved `0x5000..0x5fff` | Protected from APP writer | Review actual board memory setup |
| APP `0x6000..0x7ffff` | **488 KiB maximum**; variable-length ARM-linked binary | No fixed 488 KiB image requirement |
| Boot launch | `app_validity.c` and M33 vector/VTOR handoff | No full-image integrity marker |
| Actual FW1 image | `AM13E_FW1_REL17.elf` → raw BIN → flat BIN + 0–3 byte alignment pad | Silicon startup pending |
| Legacy block framing | Original `CMD_READ/WRITE` CRC, 1 KiB logical / 2 KiB RMW | 1KiB frame CRC is not image CRC |
| Extended addressing | `CMD_WINDOW=6`; Host tests blocks 256–487 | Real WiFi-Link updater extension unverified |
| Boot self-update `CMD_UPDATE` | **Not implemented; RES_ERROR** | Must be implemented/qualified |
| Persistent `CMD_SETWRP` | **Not implemented; RES_ERROR** | TI NONMAIN and reversible WRP qualification |
| Gate/Trip/UART/current | User-approved **five IO-only exclusions** | No physical motor enable |

**Update risk:** Partial APP writes can leave the original Cfg ID
and plausible vectors intact. Rel17's prescribed validity gate will
then allow attempted launch even if later application bytes are
corrupted or absent. No CRC-based final commit or automatic rollback
is present. Treat end-to-end flashing/recovery as an open requirement,
not an assurance of this Bootloader.

Refer to [APP_LINK_CONTRACT.md](APP_LINK_CONTRACT.md) for the selected
link/transport ABI and [REL17_V14_SOURCE_GAP_AUDIT.md](REL17_V14_SOURCE_GAP_AUDIT.md)
for original-source capability coverage and unresolved adapter work.
