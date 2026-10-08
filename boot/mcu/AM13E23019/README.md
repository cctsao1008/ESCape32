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
The remaining question is how to implement its byte timing and RX/TX
turnaround on AM13 peripherals, not which pin the Boot service uses.

## Current software baseline

Implemented and build-validated:

- dedicated Boot and application linker placement;
- common ESCape32 command engine;
- common ESCape32 service framing;
- AM13 service adapter;
- fixed PB14 physical-transport module boundary;
- application vector/reset sanity check;
- image-validity policy layer with future header/CRC pseudocode;
- application-to-Boot request layer with retained-request pseudocode;
- direct Cortex-M33 VTOR/MSP/reset-entry handoff;
- application-only read/write range mapping;
- Bank1 Flash erase/program through TI DriverLib;
- Bank0 active-bank erase/program transaction with the critical Flash command
  executor linked in `.TI.ramfunc` / RAM_C;
- readback verification.

The common protocol engine preserves the upstream command IDs and the
PROBE / INFO / READ / WRITE command flow. UPDATE and SETWRP remain optional
platform/manufacturing extensions.

## Detailed design remaining

- PB14 IOMUX and RX timing/capture implementation;
- PB14 TX waveform generation and RX/TX turnaround;
- exact service receive timeout;
- stable AM13 device-ID value for INFO;
- application-to-Boot retained request encoding and storage;
- image header / integrity / valid-record representation;
- Flash protection policy;
- optional bootloader self-update;
- on-target validation of PB14 service, Bank0/Bank1 Flash P/E, reset, APP jump,
  and recovery behavior.
