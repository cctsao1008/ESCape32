> **SUPERSEDED — ARCHIVE ONLY (former v1.6 / v2 Boot-Image ABI).**
> This file preserves earlier host-validation notes for traceability.
> Its APP metadata/header/CRC/signature-last requirements, file paths,
> fixed 256 KiB transport assumptions and completion claims are **not
> current** and **MUST NOT** be used to implement, validate, or update
> the Rel17 v1.4 firmware. Current contract:
> [README.md](README.md),
> [APP_LINK_CONTRACT.md](../../../mcu/AM13E/APP_LINK_CONTRACT.md).
> Original text follows unchanged; statements in present tense below
> describe historical states, not today's active branch.

---

# AM13E reference / AM13E Host Validation

## Stage D1: actual ESCape32 CMD_WRITE protocol over mock transport

This host test now compiles the **real production sources**:
`boot/src/main.c` (entry renamed only in the host wrapper),
`boot/src/io.c` (recvval/recvdata/senddata),
`boot/mcu/AM13E/flash_range.c`,
`boot/mcu/AM13E/flash.c` and
`boot/mcu/AM13E/image_integrity.c`.

The mocked boundary is **only** the PB14 byte-stream transport, TI Flash
controller and CRCP engine. Host CRC-32/ISO-HDLC software emulates protocol
CRC; equivalence of TI's actual CRCP configuration remains to be proven.

With the Stage C2 packed ARM image already built:

```bash
cmake -S boot/tests/am13e_host -B build-am13e-host-tests \
  -DCMAKE_C_COMPILER=gcc \
  -DAM13E_HOST_PACKED_IMAGE="$PWD/build-am13e/AM13E_APP_SMOKE.am13e-smoke.bin"
cmake --build build-am13e-host-tests -j"$(nproc)" && \
ctest --test-dir build-am13e-host-tests --output-on-failure -V
```

This adds a **sixth** CTest target `am13e_boot_protocol`. It sends
the real binary using the ESCape32 wire format:

- each command, block index and count is transmitted as
  `value, value XOR 0xFF` (two octets);
- `CMD_WRITE=3` carries `count=(bytes/4)-1`, data and a little-endian
  CRC32; a complete 1-KiB block therefore uses `count=0xFF`;
- the test covers `CMD_PROBE`, `CMD_INFO`, out-of-order block refusal,
  malformed CRC and its **absence of ACK** (current shared behavior),
  successful retry, duplicate block, deferred signature, metadata restore,
  `CMD_READ` framing and refusal of dangerous `CMD_UPDATE`/`CMD_SETWRP`;
- final programmed Flash bytes must match the actual ARM-linked packed
  image, and the whole-image validator must pass.

The AM13E-specific `recvval` branch now treats wire octets as unsigned;
this avoids signed-`char` mis-decoding of `0xFF` (full 1024-byte
payload count) on toolchains with `-fsigned-char`. Legacy MCU branches
are unchanged.

**Caution:** neither this test nor any previous stage has validated
physical PB14 GPIO timing, 38400-baud open-drain behavior, an actual
WiFi-Link programmer, the TI CRCP peripheral, power-loss electrical
behavior, or production motor-control firmware. This is a command/framing
**host** integration test, not a successful on-board firmware update.

## Stage C3: real ARM-linked packed image through production flash.c

Stage C2 produces a linked ARM image with its **physical** Reset Handler
at 0x6811 (subject to ARM compiler output). Stage C3 tests that image against
the **actual production** `boot/mcu/AM13E/flash.c` transaction implementation,
via the native-GCC Mock Flash controller.

The host maps Flash at `0x10006000` because Linux cannot directly map
the MCU's low Flash addresses. The validator now checks Reset_Handler
against the fixed **physical** AM13E reference APP base `0x6000`, not the host
pointer address. The ordinary host fixtures were adjusted accordingly.

After successful Stage C2 ARM link + pack:

```bash
cmake -S boot/tests/am13e_host -B build-am13e-host-tests \
  -DCMAKE_C_COMPILER=gcc \
  -DAM13E_HOST_PACKED_IMAGE="$PWD/build-am13e/AM13E_APP_SMOKE.am13e-smoke.bin"
cmake --build build-am13e-host-tests -j"$(nproc)"
ctest --test-dir build-am13e-host-tests --output-on-failure -V
```

This opts in a **fifth** CTest target,
`am13e_linked_image_transaction`. It reads the actual packed image file,
executes invalidation writes, streams 1-KiB-aligned blocks (including the
short 32-byte final block), stages/restores metadata, tests duplicate
metadata block 0, compares Flash byte-for-byte, checks full image CRC and
validates a simulated post-reset launchable image. It does not rewrite
the application vector or CRC to fit the host mmap address.

For a quick independent run after host build:

```bash
./build-am13e-host-tests/am13e_flash_transaction_test --flash-image \
  build-am13e/AM13E_APP_SMOKE.am13e-smoke.bin
```

This remains host emulation: AM13E Flash ECC, active-bank RAM execution,
PB14 UART framing and actual reset/handoff are NOT exercised. The input is
a *smoke ELF*, not production motor-control Firmware.

## Stage C2: ARM linker + v2 image smoke (opt-in)

The existing ESCape32 root CMake now offers an isolated **ARM ELF**
address-layout smoke target. This is not motor firmware, a board-specific
application, or a hardware-qualification image.

From the repository root in the existing WSL toolchain:

```bash
git switch am13e-port-v2
git pull --ff-only
cmake -S . -B build-am13e -DAM13E_ENABLE_APP_SMOKE=ON
cmake --build build-am13e --target AM13E_APP_SMOKE -j"$(nproc)"
```

The build should create these in `build-am13e/`:

- `AM13E_APP_SMOKE.elf`: Cortex-M33 cross-linked ELF;
- `AM13E_APP_SMOKE.map`: linker placement map;
- `AM13E_APP_SMOKE.bin`: APP-base flat raw BIN starting at `0x6000`;
- `AM13E_APP_SMOKE.am13e-smoke.bin`: Stage C1 packed and verified image;
- `AM13E_APP_SMOKE.am13e-smoke.json`: image metadata manifest.

This smoke target uses `objcopy --remove-section=.data` because the
zero-length ARM linker `.data` section is marked `ALLOC, LOAD` at
`0x20000000`. Without excluding it, GNU objcopy's `--gap-fill=0xff`
can expand a `0x6000`-based flat binary to 536,846,336 bytes.
The smoke linker simultaneously asserts `SIZEOF(.data) == 0`.
This exclusion is **only valid for this initialized-data-free smoke**;
a future real application with nonempty initialized `.data` needs its
load address placed inside Flash and included in the packed image.

This CMake target enforces linker assertions and runs a verification
script that parses `arm-none-eabi-objdump -h` (signature `0x6000`,
header `0x6100`, actual M33 vectors `0x6800`, ARM code after the
vectors) and compares ELF/objcopy/packed image bytes and CRCs. It must
not be reported as PASS until the actual WSL command succeeds.

The smoke uses `boot/mcu/AM13E/linker_app_smoke.ld` and
`boot/tests/am13e_app_link_smoke.c`. It contains a dummy WFI
`Reset_Handler`, deliberately **not** an initialized application or
a motor-control program. Do not flash or execute it as firmware.

To opt back out:

```bash
cmake -S . -B build-am13e -DAM13E_ENABLE_APP_SMOKE=OFF
```

Next production step is to take the v2 metadata/reserved-sector/vector
layout into a **real AM13E Rel17 APP link**, with verified startup,
SysConfig, pin mapping and motor output safety, and to verify it against
the existing hardware bootloader.

## Stage C1: Image Pack / Verify (host-only)

The new `boot/tools/pack_am13e_v2.py` packs **only v2 APP-base flat
raw binaries**, not old vector-first `.am13e.bin` files, and does **not**
flash the MCU.

Expected raw binary input:
- First file byte represents physical APP base `0x6000`, **not** the
  start of an ELF section or the M33 vector table itself.
- First two bytes at APP+0 must be erased `FF FF` or already contain
  ESCape32 `EA 32`. The packer writes the final `EA 32`.
- Metadata slot at APP+0x100 (32 bytes) must be all `FF` or all `00`.
- M33 Vector Table at APP+0x800 (physical address `0x6800`) must
  contain a valid secure SRAM stack pointer and Thumb Reset Handler.
- Max packed image length 256 KiB; image is padded to 16-byte boundary
  with `FF`. Original file length is not recoverable separately after
  padding; `image_length` in metadata includes padding.

From the repository root:

```bash
python3 boot/tools/pack_am13e_v2.py pack \
    path/to/app-flat-at-0x6000.bin path/to/app-v2.am13e.bin \
    --manifest path/to/app-v2.am13e.json

python3 boot/tools/pack_am13e_v2.py verify path/to/app-v2.am13e.bin
```

The `am13e_image_packer` CTest performs CLI roundtrip,
metadata/CRC-byte coverage checks, alignment, old format rejection,
bounds, corruption and truncation.

**Important:** the `AM13E` application target in the current
`am13e-port-v2` root CMake is still an **OBJECT library**. It
does not yet produce a linked, address-qualified v2 application ELF
or v2 raw binary. Do not take an arbitrary `objcopy -O binary`
output and assume it starts at APP base `0x6000`.
A v2 Application Linker and its app binary generation remain Stage C2.
No linked application artifact or live ESC firmware update has been
qualified by this host-only packer.

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

## Image contract — **v2**, not the old vector-first .am13e.bin

- APP base = `0x6000`.
- ESCape32 legacy signature (`0x32EA`, little endian) = APP + 0.
- AM13E reference 32-byte header = APP + `0x100`.
- Cortex-M33 initial MSP + Reset Handler = APP + `0x800`.
- Header layout uses AM13E reference v1's magic/version/target/length/CRC/flags
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

An earlier AM13E reference image with vectors at `0x6000`, header at `0x6100`
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
