# AM13E v1.6 Boot / FW1 Integration Status

**Updated 2026-10-10: prior APP_BASE/vector and metadata mismatches are
resolved in Source and Host CI.** This record supersedes the older
Boot-v2-only audit; see `APP_LINK_CONTRACT.md` for exact firmware layout.

| Integration topic | Implemented | Evidence / remaining boundary |
|---|---|---|
| APP_BASE/vector | Both FW1 and common Boot use `0x6000` | Linked ELF sections/symbols, ARM startup, host validator |
| Signature/header CRC | Signature `+0x400`; header `+0x500` | Image packer + actual Boot CRC checks |
| Single APP allocation | `0x6000..0x7FFFF` (488 KiB) | `linker_app_v16.ld`; current 256 KiB transport cap remains |
| FW1/FW2 params | Dedicated `0x4000..0x4FFF` and `0x5000..0x5FFF` | Link assertions, separated FW1 Flash Writer |
| Firmware update | APP-only erase/program/verify, signature-last | Boot Protocol and Flash Transaction Host Tests |
| Boot handoff | VTOR `0x6000`, valid SP/PC, direct launch | `boot/mcu/AM13E/app.c`, actual Boot ELF |
| Flash P/E | TI DriverLib Flash routines and SRAM RAMFUNC | ARM ELF/MAP; power-failure silicon behavior awaits HW |
| FW1 and Boot build | Real `AM13E_FW1_V16.elf` and `BOOT5_PB14.elf` | GitHub Actions Run #38059589632 |
| FW1 Packed Image | 33,968 bytes, full CRC | Real packed-image Boot Host tests, all 6 PASS |
| FW2 source | Separate TI Sensorless FOC implementation | FW2 is **not** implemented by this FW1 port |
| Hardware qualification | Not started | Deliberately deferred until software port complete |

The legacy `mcu/AM13E/linker_app_reference.ld` and
`boot/mcu/AM13E/linker_app_smoke.ld` retain their named roles as
**historical diagnostic** and **vector-first smoke** respectively;
do not mistake either for the actual FW1 linker.

The Baseline explicitly defers image marker/CRC **format** to detailed
design. The new `+0x400/+0x500` metadata placement is that design choice,
not a silent change to the architecture. No change to WiFi-Link's 1KiB
block protocol or single-image FW1/FW2 selection was introduced.

See **APP_LINK_CONTRACT.md** and the CI logs before changing packaging,
STM32 compatibility logic, or Boot/App startup. 
