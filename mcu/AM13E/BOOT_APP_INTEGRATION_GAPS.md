# ESCape32 AM13E — Boot / Application Integration Gaps

**Status:** Source-backed interface audit; **NOT** a production linker
approval, Boot implementation change or Flash protocol change.

## Source-of-truth order

- Product SW Architecture Baseline v1.6: Flash regions, update
  semantics, FW1/FW2 parameter ownership and Application contract.
- Product HW Architecture Baseline v1.6: pins and peripheral functions.
- ESCape32 Rel17 Application and root `CMakeLists.txt`:
  canonical Application code and build.
- TI SDK / CMSIS / DriverLib: target MCU support only
  (analogous to libopencm3); not a second Application owner.
- Current AM13E Boot: inspected implementation/reference,
  **not authority to change the SW architecture**.

## Confirmed implementation facts and reconciliation gates

| Gate | SW v1.6 required behavior | Present Boot / Application reference | Status |
| --- | --- | --- | --- |
| Flash partitions | Boot `0x0000..0x3FFF`; FW1 cfg `0x4000..0x4FFF`; FW2 cfg `0x5000..0x5FFF`; single APP `0x6000..0x7FFFF` (488 KiB) | Older reference linker reserves combined 8 KiB `FLASH_CFG` and only builds App Flash through `0x45FFF` | **Mismatch**; linker must model split cfg and entire 488 KiB allocation |
| Transport addressing | Reflash a single Application over its allocated region, preserving both cfg regions | `boot/src/main.c` gets block number with one-byte `recvval()`. `boot/mcu/AM13E/flash_range.c` rejects `block > 255`, with 1024-byte blocks | **P0 blocker** for full 488 KiB: would require indices 0..487, outside 8-bit protocol. Do not merely increase a C range constant |
| Image length validator | Image integrity and Application capacity compatible with v1.6 | `boot/mcu/AM13E/image_integrity.h` defines `AM13E_IMAGE_MAX_TRANSPORT_BYTES=(256*1024)`; `image_integrity.c` enforces it | **P0 blocker**: new host/Boot/validator length policy must be agreed and implemented together |
| APP entry and vector | Fixed APP_BASE `0x6000`, valid vector/startup entry at Application base | `image_integrity.h` uses APP_BASE `0x6000`, but `AM13E_IMAGE_VECTOR_OFFSET=0x800`; `app.c` sets VTOR to `0x6800`; old linker places vectors at `0x6800`; header at `0x6100` | **Format contract unresolved**: reconcile image base, vector location, signature/header, reset entry and packing before production linker |
| FW1/FW2 cfg independence | FW1 only 4 KiB, FW2 only 4 KiB, preserve across application update | `linker_app_reference.ld` defines `FLASH_CFG` 8 KiB with `_cfg=0x4000`; no explicit FW1 4 KiB upper bound in linker | **Risk / incomplete:** FW1 storage must be capped to 4 KiB; FW2 must never be erased during FW1 parameter commit |
| Boot application-only erase | Erase/program/verify only APP, preserve Boot and both configs | `flash_range.c` bounds reads/writes starting at `__app_flash_start__`; good separation at entry, subject to validated linker symbols and erase-sector bounds | **Partially aligned**; verify actual erase addresses and code paths at final Link/Hardware gates |
| TI RAMFUNC | Safe Flash operations from SRAM where required | Real `DL_FRI_setReadWaitStates` linked in a non-flashable synthetic fixture at SRAM_C VMA with separate Flash LMA | **Mechanism verified**, not actual ESCape32 Application or SRAM flash-program call graph |

### Evidence locations (current branch)

- `boot/src/main.c`: `CMD_READ` / `CMD_WRITE` block
  numbers use the one-byte `recvval()` wire frame.
- `boot/src/io.c`: `recvval()` returns a single byte;
  `recvdata()` decodes up to 1024 bytes per block.
- `boot/mcu/AM13E/flash_range.c`: `block > 255U`
  rejection and application-first absolute address calculation.
- `boot/mcu/AM13E/image_integrity.h`: 256 KiB limit,
  image format and vector offset constants.
- `boot/mcu/AM13E/image_integrity.c`: actual size,
  signature/vector and CRC validation.
- `boot/mcu/AM13E/flash.c`: ordered write session,
  2 KiB sector erase, signature-last commit and CRC check.
- `boot/mcu/AM13E/app.c`: Flash image validation,
  PRIMASK disabled, VTOR/MSP/Reset_Handler handoff.
- `mcu/AM13E/linker_app_reference.ld`: old smoke-only
  256 KiB/combined 8 KiB format; MUST NOT be used as production.
- `mcu/AM13E/tools/probe_app_linker.py`: tests old format only.

## Recommended sequence (do not change policy by inference)

1. Define **one** ESCape32-compatible, extended application
   update transport addressing contract capable of representing
   the 488 KiB range; assess old WiFi-Link interoperability
   and host programmer behavior. No wire format selected yet.
2. Define exact APP image entry, M33 vectors, signature,
   metadata and CRC relationship. Preserve ESCape32 application
   control flow; do not let an old smoke test choose the format.
3. Derive Boot/linker/packer parameters from the agreed contract;
   prove independent FW1/FW2 4 KiB parameter protection and
   application-only update.
4. Implement the minimum platform-specific changes inside
   the ESCape32 repository with TI DriverLib as the MCU layer.
5. Test host transport, bounds, packet retries, power-loss
   recovery, linker/startup and real final Application ELF
   as **separate** gates.

## Compile gate completed

User WSL `architecture-alignment.log` shows all **11** objects
compiled without warnings or errors after removing the
evaluation-board SysConfig include default. Five objects are
original Rel17 sources, four AM13E adapters, and two TI DriverLib
sources. This proves **ESCape32 object compilation only**;
the log does not include an object symbol inventory, final
Application Link or physical hardware verification.

**No Boot protocol, Boot firmware, image format, linker memory
assignments, power stage or customer board implementation was
changed by this audit.**
