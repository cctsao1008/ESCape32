# AM13E23019 port

This directory is the AM13E23019 platform bring-up area for the `am13e-port`
branch.

## Stage A — relocatable application baseline

The first target is deliberately small:

- TI AM13E230x SDK 26.01.00.03.STS
- SysConfig 1.28.0.4712
- Arm GNU Toolchain 15.2.rel1
- No-RTOS
- TI DriverLib / SDK startup baseline
- no ESCape32 motor-control behavior yet

The flash contract under validation is:

```text
0x00000000..0x00003FFF  Bootloader      16 KiB
0x00004000..0x00005FFF  Persistent CFG   8 KiB
0x00006000..             Application
```

For this stage the TI default GCC linker script is retained and the linker
symbol `_intvecs_base_address` is overridden to `0x00006000`. The TI linker
script places `.intvecs` first, so subsequent flash sections follow the
application vector table. The build wrapper verifies the resulting ELF section
addresses.

This is intentionally a validation mechanism, not the final flash-partition
enforcement. A dedicated application linker script should be introduced after
Boot/CFG update ownership and same-bank flash-write behavior are validated.

## Build

From the ESCape32 repository root:

```bash
./tools/am13e/build-am13e-bringup.sh
```

The wrapper seeds `example.syscfg` from the pinned TI SDK empty LaunchPad
example into a temporary build overlay. That keeps the repository free of
generated SysConfig output during this baseline stage.

Expected ELF invariants:

```text
.intvecs     0x00006000
.text        >= 0x00006100
.vtable      0x20000000
.TI.ramfunc  0x00C18000
ABI          hard-float
```
