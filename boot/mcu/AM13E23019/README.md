# AM13E23019 common bootloader

The AM13E23019 Boot port follows the upstream ESCape32 boot architecture while
keeping target-specific Flash, startup, reset, and service-I/O details in the
AM13E backend.

Current image contract:

```text
0x00000000..0x00003FFF  Common bootloader   16 KiB
0x00004000..0x00004FFF  FW1 parameters       4 KiB
0x00005000..0x00005FFF  FW2 parameters       4 KiB
0x00006000..0x0007FFFF  Single application 488 KiB
```

Normal application update preserves the bootloader and both 4-KiB parameter
regions.

## Current software baseline

Implemented and build-validated:

- dedicated boot and application linker placement;
- application vector/reset sanity check;
- direct Cortex-M33 VTOR/MSP/reset-entry handoff;
- application-only read/write range mapping;
- Bank1 Flash erase/program through TI DriverLib;
- Bank0 active-bank erase/program transaction with the critical Flash command
  executor linked in `.TI.ramfunc` / RAM_C;
- readback verification;
- common ESCape32 boot command engine under `boot/src/protocol.c`;
- AM13E boot platform adapter under `boot/mcu/AM13E23019/src/boot_port.c`.

The common protocol engine preserves the upstream command IDs and the
PROBE / INFO / READ / WRITE command flow. UPDATE and SETWRP remain optional
platform/manufacturing extensions.

## Next work

The next implementation step is the generic service-I/O backend required to
connect the AM13E target to `boot_protocol_run()`.

The physical service binding must stay outside the common command engine.
PB14/GPIO46 is the current runtime PWM/DShot/BiDShot pin, but its reuse for
service/update remains subject to the product HW architecture / TI review.

Still open:

- service-I/O backend and physical binding;
- application-to-boot entry request encoding;
- image header / integrity / valid-record representation;
- Flash protection detailed design;
- optional bootloader self-update;
- on-target validation of Bank0/Bank1 erase, program, verify, reset, and
  recovery behavior.
