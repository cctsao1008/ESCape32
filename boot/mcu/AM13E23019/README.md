# AM13E23019 common bootloader

The AM13E23019 Boot port follows the upstream ESCape32 boot architecture while
keeping target-specific Flash, startup, service, image-policy, and boot-entry
details in the AM13 backend.

See `ARCHITECTURE.md` for the complete code-level flow and pseudocode for
detailed-design items that are intentionally not implemented yet.

Current image contract:

~~~text
0x00000000..0x00003FFF  Common bootloader   16 KiB
0x00004000..0x00004FFF  FW1 parameters       4 KiB
0x00005000..0x00005FFF  FW2 parameters       4 KiB
0x00006000..0x0007FFFF  Single application 488 KiB
~~~

Normal application update preserves the bootloader and both 4-KiB parameter
regions.

There is no A/B image, bank swap, or bank-level firmware selection. E62 treats
MAIN Flash as one contiguous product/update address space; physical Flash-bank
boundaries matter only to the AM13 low-level program/erase implementation.

## Architecture baseline

~~~text
common ESCape32 command engine
        |
common ESCape32 framing
        |
AM13 service adapter
        |
PB14/GPIO46 service transport
        |
AM13 application / Flash backend
~~~

PB14/GPIO46 is the fixed physical service interface for this AM13 port.
Boot uses a polling 38400-baud 8N1 software-UART with SysTick timing from a
deterministic 32-MHz SYSOSC basis. Runtime DShot/BiDShot remains a separate
application-side timing implementation.

## Current software baseline

Implemented and build-validated:

- dedicated Boot and application linker placement;
- common ESCape32 command engine;
- common ESCape32 service framing;
- AM13 service adapter;
- deterministic 32-MHz Boot platform/clock basis;
- fixed PB14 38400-baud polling service transport;
- application vector/reset sanity check;
- image-validity policy layer with future header/CRC pseudocode;
- application-to-Boot one-shot request stored in SYSCTL SHUTDNSTORE bytes;
- direct Cortex-M33 VTOR/MSP/reset-entry handoff;
- application-only read/write range mapping;
- contiguous MAIN-Flash erase/program handling;
- normal TI DriverLib P/E when no execution-bank conflict exists;
- RAM_C-resident same-bank P/E transaction when the target bank contains
  currently executing Boot code, with the critical Flash command executor
  linked in `.TI.ramfunc`;
- readback verification.

The common protocol engine preserves the upstream command IDs and the
PROBE / INFO / READ / WRITE command flow. UPDATE and SETWRP remain optional
platform/manufacturing extensions.

## Detailed design remaining

- PB14 electrical/timing validation on E62 hardware;
- image header / integrity / valid-record representation;
- Flash protection policy;
- optional bootloader self-update;
- on-target validation of PB14 service, MAIN-Flash P/E across the full APP
  region, reset, APP jump, and recovery behavior.
