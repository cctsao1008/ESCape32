# E62 software release-gate corrections v1

Base firmware: 53953c04f7d857f3b402b3a19870605ed0471d8f.
SDK: b872469ceef18de5e39a1d3acb0821ec158b44c7.
ARM GCC: Debian 14.2.1 20241119; binutils 2.44; CMake 3.31.6.

Two reproducible gate defects were corrected without changing motor control:

- Boot's AM13E CMD_SETWRP arm declared `requested` immediately after a case
  label, rejected by `-Wpedantic -Werror`. A compound statement makes the same
  control flow valid C. Other MCU implementations remain unchanged.
- The flash host script did not provide the production app_validity include
  directory or link its implementation, although its existing fixture calls
  `boot_am13e_app_validity`. Both are now explicit.

Commands (SDK and build paths may be absolute):

```sh
cmake -S . -B BUILD -G Ninja -DESCAPE32_BUILD_AM13E=ON \
  -DAM13E_SDK_ROOT=SDK -DAM13E_BUILD_DIAGNOSTICS=ON
cmake --build BUILD --target AM13E BOOT5_PB14-rev4.elf \
  AM13E_PORTABLE_LOGIC AM13E_MCU_FAULT_TRIP_BACKEND AM13E_MCU_RUNTIME_TICK \
  AM13E_MCU_ADC_PAIR_BACKEND AM13E_MCU_COMMAND_INPUT_ROUTE \
  AM13E_MCU_MOTOR_PAD_BACKEND -j3
python3 mcu/_support/AM13E/tools/verify_rel17_image.py --build BUILD
AM13E_SDK_ROOT=SDK bash mcu/_support/AM13E/tools/run_host_regressions.sh
BUILD_DIR=HOST_BUILD bash mcu/_support/AM13E/tests/boot/run_am13e_flash_host_tests.sh
HOST_BUILD/am13e_flash_transaction_test --flash-image BUILD/AM13E-rev17.3.bin
```

Results: all listed ARM compile/link targets PASS; actual Boot/application/flat
image audit PASS; 107 full host groups PASS; flash test PASS with ASan/UBSan,
including the newly linked application BIN, block writes, reserved/CFG/Boot
preservation, failure injection/retry and validity checks. LeakSanitizer requires
execution outside the restricted command sandbox; it was not disabled.

Limitations: the adopted Rel17 transport/validity interface does not provide
full-image CRC, interruption-completeness or transactional rollback guarantees.
The passing tests explicitly retain these limitations. A signed-off board,
programmer, measured independent faults, 47V bench safety, thrust/current/thermal
measurements and release freeze remain external gates. `porting_complete=false`
and `hardware_qualified=false`; no release or production approval is claimed.

A separate toolchain observation: GNU ld 2.44's default 64KiB ELF load-page
alignment can map an APP PT_LOAD segment down to zero, overlapping a separate
CFG loader. Using the supported linker option `-Wl,-z,max-page-size=0x1000`
keeps the APP ELF segment at 0x6000 without changing application instructions.
Raw BIN programming is distinct from loading ELF segments. This is recorded
as a source-built-QEMU integration constraint, not hidden by a simulator waiver.
