# E62 / AM13E Host Validation

## Stage B: production image integrity integration

The current `am13e-port-v2` build links the **same portable validator**
(`boot/mcu/AM13E/image_integrity.c`) into all three relevant paths:

1. Host validator fixture (`am13e_image_validator`): validates the header,
   signature, M33 vector, length and CRC-32/ISO-HDLC.
2. Real AM13E `flash.c` (`am13e_flash_transaction`): after ordered blocks
   2..N and final metadata blocks 0 and 1 are programmed and verified, the
   16-byte signature remains in SRAM. The backend validates the complete
   image with the staged 16-byte prefix and requires the header length
   to **exactly match the number of bytes delivered by this transaction**
   before committing signature.
3. Real AM13E `app.c`: cold boot / application handoff must pass the
   *committed-image* CRC check before executing the Cortex-M33 application.

The formerly RED `am13e_image_integrity_gate` is now a **mandatory CTest**
with three checks: early finalization with missing data; early finalization
when stale Flash bytes make CRC appear valid but the transfer is incomplete;
and a full-length transfer with corrupted content. Each must leave the
application signature invalid.

## Build commands

Run in WSL, from the repository root:

```bash
git switch am13e-port-v2
git pull --ff-only

cmake --build build-am13e --target BOOT5_PB14.elf -j"$(nproc)"

cmake -S boot/tests/am13e_host -B build-am13e-host-tests \
  -DCMAKE_C_COMPILER=gcc
cmake --build build-am13e-host-tests -j"$(nproc)"
ctest --test-dir build-am13e-host-tests --output-on-failure -V
```

Use `&&` between build and CTest when running as one compound shell command;
otherwise CTest could still run a previously built executable after a
compilation error.

CTest targets:
- `am13e_flash_transaction`: 10 normal/fault/reset transaction cases;
- `am13e_image_validator`: 9 standalone image/CRC cases;
- `am13e_image_integrity_gate`: 3 negative end-to-end finalize conditions.

No additional shell-based build workflow is needed.

## Image contract — **v2**, not the old vector-first .e62.bin

- APP base = `0x6000`.
- ESCape32 legacy signature (`0x32EA`, little endian) = APP + 0.
- E62 32-byte header = APP + `0x100`.
- Cortex-M33 initial MSP + Reset Handler = APP + `0x800`.
- Header layout uses E62 v1's magic/version/target/length/CRC/flags
  field definitions, but **the placement of vectors differs**.
- Header CRC = first 28 bytes of the header.
- Image CRC = image bytes `[0,image_length)`, excluding the 32-byte
  header at offset `0x100`; the CRC *includes* the eventual signature,
  supplied from SRAM for the precommit check.
- CRC-32/ISO-HDLC: reflected polynomial `0xEDB88320`, init/xorout
  `0xFFFFFFFF`.
- Length must be 16-byte aligned and no more than 256 KiB, imposed
  by the current 8-bit / 1 KiB protocol address space. This is distinct
  from the physical 488 KiB APP partition.
- Legacy signature-last transactions: invalidate 0/1, write blocks 2..N
  sequentially, restore metadata blocks 0 and then 1; `0x32EA` only
  becomes visible after successful whole-image verification.

An earlier E62 image with vectors at `0x6000`, header at `0x6100`
**cannot** be installed as a v2 boot image unchanged. A compatible v2
application linker and packager are still required.

## Qualification limits

- Host tests run the production `flash.c` and `image_integrity.c`
  with a RAM-backed Mock DriverLib. They do not simulate Flash ECC,
  Bank0 active-bank command safety, RAM ISR behavior, reset/power-cycle
  electrical effects or PB14 UART timing.
- CRC protects against accidental truncation/corruption, **not**
  malicious or unauthorized firmware; it is not a digital signature.
- In-place single-bank programming can erase the old application at
  session start. A failed transfer requires Boot recovery; it cannot
  guarantee old-image rollback.
- Real firmware artifact + image packer interoperability,
  16-byte Flash programming with ECC, and actual hardware power-loss
  testing remain open gates.

Do not claim Hardware Bring-up or End-to-End Firmware Update PASS on
the basis of these native-GCC tests.
