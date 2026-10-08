# AM13E Host Flash Transaction Validation

## Scope

The native-GCC harness compiles the production `boot/mcu/AM13E/flash.c` with
`AM13E_FLASH_TEST` and substitutes Flash DriverLib/CMSIS operations. It verifies
software transaction behavior, **not** AM13E Flash Controller hardware semantics.

The 10 ordinary transaction tests cover invalidation, ordered writes, retries,
short tails, signature deferral, erase/program faults, and two simulated
volatile-state resets. A reset in this suite leaves the emulated Flash memory
intact; this is not an MCU power-cycle test.

## Green regression gate

From the repository root:

```bash
cmake -S boot/tests/am13e_host -B build-am13e-host-tests -DCMAKE_C_COMPILER=gcc \
  -DAM13E_ENABLE_IMAGE_INTEGRITY_GATE=OFF
cmake --build build-am13e-host-tests -j
ctest --test-dir build-am13e-host-tests --output-on-failure -V
```

This runs **10 transaction checks in one CTest executable**.

## Known-open integrity gate (opt-in, expected RED)

The current implementation can restore the legacy `0x32EA` signature after
only one data block because there is no authoritative image length and
whole-image CRC check before metadata finalization.

A separate negative regression gate demonstrates this *contract gap* using a
synthetic image for which more data blocks are expected. The current
implementation cannot distinguish this truncated transfer from a legitimately
short image. It is intentionally **not part of the green regression suite**.

```bash
cmake -S boot/tests/am13e_host -B build-am13e-host-tests -DCMAKE_C_COMPILER=gcc \
  -DAM13E_ENABLE_IMAGE_INTEGRITY_GATE=ON
cmake --build build-am13e-host-tests -j
ctest --test-dir build-am13e-host-tests -R '^am13e_image_integrity_gate$' \
  --output-on-failure -V
```

A failing integrity gate is evidence of an unresolved safety requirement,
**not** a new compiler failure or a failure of the 10 transaction tests.
Disable it afterward with
`cmake -S boot/tests/am13e_host -B build-am13e-host-tests -DAM13E_ENABLE_IMAGE_INTEGRITY_GATE=OFF`.

## Image-contract decisions required

1. **Format conflict:** the current v2 bootloader checks `0x32EA` at
   `__app_flash_start__` and expects the vector table one 2 KiB sector later.
   Earlier E62 packed-image logs specify vector at `0x6000` and E62 header at
   `0x6100`. Do not claim these images are interchangeable.
2. **Address span:** `flash_range.c` limits the protocol block number to
   0..255 in 1 KiB units (256 KiB addressable by the current command).
   The previously planned E62 application partition is larger (488 KiB).
   Decide whether to constrain the image or extend addressing without
   changing legacy targets.
3. **Integrity:** define an authoritative image length, CRC algorithm,
   protected byte span, metadata placement, commit conditions, and packager/host
   agreement before accepting any signature-based update as production-safe.
4. **Hardware:** confirm 2 KiB erase / 16-byte program / ECC / RAM execution,
   actual reset and UART protocol on AM13E23019.

Do not reinterpret Host Test PASS as Hardware or Image Integrity PASS.
