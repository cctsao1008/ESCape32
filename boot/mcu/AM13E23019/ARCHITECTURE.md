# AM13E23019 Boot Port Architecture

This document defines the code-level architecture of the AM13E23019 ESCape32 boot port.
It intentionally includes pseudocode for detailed-design items that are not ready for
hardware implementation yet.

The goal is to keep the complete boot flow visible while preserving a clean boundary
between upstream ESCape32 semantics and AM13E-specific realization.

## 1. Architecture

~~~text
Reset / startup
     |
     v
boot/mcu/AM13E23019/src/main.c
     |
     +--> boot_platform
     |      deterministic SYSOSC 32-MHz Boot clock basis
     |
     +--> boot_request
     |      application -> boot one-shot request
     |
     +--> boot_service_pb14
     |      PB14/GPIO46 physical byte transport
     |      RX/TX timing implementation
     |
     +--> boot_service_port
     |      AM13 transport -> common ESCape32 adapter
     |
     +--> boot/src/service_io
     |      complement framing + payload framing + CRC
     |
     +--> boot/src/protocol
     |      PROBE / INFO / READ / WRITE
     |      optional UPDATE / SETWRP
     |
     +--> boot_image
     |      vector sanity now
     |      image record / CRC later
     |
     +--> boot_port
     |      APP map / APP write-block policy / Cortex-M33 handoff
     |
     +--> boot_flash
            contiguous MAIN-Flash erase/program/verify
            RAM-resident same-bank transaction where required
~~~

## 2. Ownership Boundaries

### Common ESCape32 layer

boot/src/protocol.c owns command semantics:

- PROBE
- INFO
- READ
- WRITE
- UPDATE extension hook
- SETWRP extension hook
- application fallback behavior

boot/src/service_io.c owns transport-independent ESCape32 framing:

- value + complement encoding
- block-length encoding
- payload CRC framing

These files must not know AM13 registers, PB14, Flash-bank layout, or the
application address.

### AM13E23019 platform layer

boot_port.c owns:

- fixed APP mapping
- read/write block translation
- APP vector handoff
- target-specific application access policy

boot_flash.c owns:

- APP-range enforcement
- 2-KiB sector erase
- normal DriverLib path when execution-bank conflict does not exist
- RAM_C transaction when program/erase targets the bank containing Boot code
- Flash readback verification

boot_service_port.c owns:

- adaptation from AM13 byte transport to common framing/protocol
- AM13 software CRC implementation
- AM13 device-info callback boundary

boot_service_pb14.c owns the fixed AM13 physical service interface:

~~~text
External service line
        |
3.3-V / 5-V tolerant bidirectional front end
        |
PB14 / GPIO46
        |
polling software UART
        |
SysTick timing @ deterministic 32-MHz SYSOSC
~~~

The Boot service link is 38400 baud, 8N1, LSB-first, idle-high, with a
500-ms receive timeout. This is intentionally simpler than the application
runtime DShot/BiDShot timing path; PB14 remains the shared physical pin, but
Boot does not need to reuse the runtime eCAP/DMA implementation.

boot_request.c owns the application-to-Boot one-shot request contract.

boot_image.c owns application launch-validity policy.

## 3. Reset / Boot Flow

Current architecture pseudocode:

~~~text
on reset:
    request = detect_boot_request()

    if request exists:
        consume_request_once()

    initialize PB14 service transport

    if PB14 transport is operational:
        bind PB14 -> service framing -> common protocol
        run common protocol

        on receive timeout / invalid command:
            if application is launchable:
                jump application

        on WRITE:
            map block into APP region only
            erase required 2-KiB sector
            program data
            verify data

    validate installed application

    if application is launchable:
        set VTOR
        set MSP
        branch reset entry

    otherwise:
        remain in Boot / recovery
~~~

## 4. Application-to-Boot Request

Detailed design is not frozen yet. Required behavior is already fixed:

~~~text
Installed FW1/FW2
    |
service requests firmware update
    |
force motor/power stage to safe state
    |
stop runtime command handling
    |
write one-shot retained boot request
    |
system reset
    |
Boot reads + validates request
    |
Boot clears request
    |
PB14 programming/service mode
~~~

The retained request encoding now uses SYSCTL SHUTDNSTORE0..3 with
magic/complement and reason/complement bytes. SHUTDNSTORE is retained across
SYSRST, and Boot clears a valid request before entering the service path to
preserve one-shot behavior.

## 5. Image Validity

Current implemented launch gate:

~~~text
valid initial MSP in RAM_S
AND
Thumb reset entry
AND
reset entry inside APP region
~~~

Planned policy:

~~~text
vector sanity
AND
image header target/version/bounds
AND
payload CRC
AND
completed-update valid record
~~~

The parameter regions must not be used as the sole application-valid marker
because they are preserved during application reflashing.

## 6. PB14 Service Transport

The physical pin is fixed for this port:

~~~text
ESC_CMD / PWM_IN / DSHOT_BIDIR / SERVICE
    -> PB14 / GPIO46
~~~

Current Boot implementation:

~~~text
RX:
    PB14 GPIO input + pull-up
    wait for start-bit low with 500-ms timeout
    sample start/data/stop bits using SysTick delays

TX:
    preload idle-high
    enable PB14 GPIO output
    emit start + 8 data + stop bits
    release back to input after final stop bit
~~~

The implementation follows the proven E61-TI software-UART approach but uses
the AM13 reset-default 32-MHz SYSOSC and SysTick as the Boot timing base.
Electrical-level behavior and baud/timing margin still require E62 hardware
validation.

## 7. Flash Update Flow

Normal application update:

~~~text
Boot region        preserve
FW1 parameters     preserve
FW2 parameters     preserve
APP region         erase / program / verify
~~~

E62 treats MAIN Flash as one contiguous address space for the product/update
model. The application is one contiguous region from 0x00006000 through
0x0007FFFF; there is no A/B image concept, bank swap, or bank-level firmware
selection.

The AM13 device still has two physical Flash banks internally. That distinction
is used only inside the low-level Flash backend because program/erase of the
bank containing currently executing Boot code requires the RAM-resident
transaction path. It does not create a product-level partition or update mode.

## 8. Current Implementation Status

| Area | Status |
|---|---|
| Common ESCape32 command engine | Implemented / regression build PASS |
| Common service framing | Implemented / regression build PASS |
| AM13 application map / handoff | Implemented / build PASS |
| APP-only update guard | Implemented / build PASS |
| MAIN Flash non-conflicting P/E path | Implemented / build PASS |
| MAIN Flash same-bank RAM P/E path | Implemented / static placement PASS |
| Flash verify | Implemented / build PASS |
| Boot platform / deterministic 32-MHz clock basis | Implemented / HW-Pend |
| PB14 physical module boundary | Implemented |
| PB14 38400-baud RX/TX timing | Implemented / HW-Pend |
| Boot-entry retained request | Implemented / HW-Pend |
| Image header / CRC / valid record | Pseudocode |
| Flash protection policy | Detailed design |
| Optional Boot self-update | Deferred |
| Target-board validation | Pending hardware |
