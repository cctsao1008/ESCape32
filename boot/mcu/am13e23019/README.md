# E62 AM13E23019 common bootloader

Stage Boot-A establishes the fixed boot/application image contract before
service transport and Flash programming are added.

```text
0x00000000..0x00003FFF  Common bootloader   16 KiB
0x00004000..0x00004FFF  FW1 parameters       4 KiB
0x00005000..0x00005FFF  FW2 parameters       4 KiB
0x00006000..0x0007FFFF  Single application 488 KiB
```

Boot-A validates the application vector/reset entry and performs a direct
Cortex-M33 handoff by updating VTOR, MSP, and branching to the application's
reset entry.

Intentionally pending:

- PB14 service/programming transport;
- boot-entry request encoding;
- image header/integrity/valid-record representation;
- application erase/program/verify;
- Flash protection policy;
- optional bootloader self-update.

Normal application update must preserve both 4 KiB parameter regions.
