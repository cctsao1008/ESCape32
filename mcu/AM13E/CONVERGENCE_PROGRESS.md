# Rev1.4 software convergence evidence

Authority: original Rel17, the three immutable `v1.4/` design documents,
then official TI SDK/TRM. Baseline: `830a0eedbf73b46174067b5c80570118e699cfeb`.
These implementation notes do not replace the design or establish hardware readiness.

## Service reset and Boot handoff

Requirements APP-I31, APP-M12, BOOT-M07/M08, A04/A14, B04/B05/B06:
**SOURCE IMPLEMENTED / SOFTWARE VERIFIED (ordering only) / HARDWARE PENDING**.
The selected CLI's existing 00 ff request now calls production
`am13e_app_service_reset`: existing motor fault-shutdown (gate inactive,
commutation/BEMF cancellation, PWM freeze, pad disconnect), barrier, system reset.
Fault-triggered reset retains its previous fail-closed halt.
Boot validates original Cfg.id/vector/SP bounds before releasing PB14, stopping
SysTick, clearing the 48 SDK-defined external IRQ enable/pending bits and pending
SysTick/PendSV, then transferring VTOR/MSP to the Application. No Flash/protection
or unrelated peripheral register is changed by this handoff cleanup.

Production-function host regression `service_handoff` checks safe-off/barrier/reset
order and repeated quiescence, including untouched unimplemented NVIC bits.
Its physical shutdown and GPIO-release boundaries are mocked; it does not test
real pad electrical state, CMSIS reset timing or execute the ARM branch.
Existing Application-validity tests cover bad marker/SP/vector refusal.

Local official GCC 15.2.1 / TI SDK 26.01.00.03.STS validation: Application/Boot
strict links, 11/11 host CTests, existing native host regression groups,
selected UART RX/CRSF TX+CLI/EXBUS TX/telemetry strict ARM links, six recovery
negative checks, image/layout audit and immutable-document audit PASS.
GitHub CI uses its existing independently configured SDK/toolchain; matching
commit CI must be checked separately and is not official-SDK provenance evidence.

## Remaining design obligations

**DESIGN REQUIRED**, pending integration: complete original input-discovery
sequence; SBUS2 scheduler; original
half-duplex transports; optional Hall/LED/BEC hooks; broader shared
motor/config/command production-path traces. Existing codec/model tests and ARM
links alone do not close these source requirements.

**BLOCKED / HARDWARE PENDING**: physical power-stage/dead-time/BEMF/clock/fault
qualification; measured input and telemetry timing; board-calibrated sensor
channels/scales; TI-supported receiver clock trim; full SRAM indirect/literal/data
closure and post-erase failure/recovery policy for Bank0 self-update; reversible
NONMAIN protection/reload and ROM BSL/SWD/power-loss recovery qualification.
All recovery gates remain fail closed. No physical operations were performed.

## Servo/Oneshot source integration

Requirements APP-I01/I02/I06/I14/I20/I26, C05, A11/A12:
**SOURCE IMPLEMENTED / SOFTWARE VERIFIED** for retained width+period,
500Hz Servo admission, original timer-domain normalization, selected-mode
28..32 telemetry requests, throttle conversion, running invalid-width refusal
without watchdog feed, and safe-off reset on idle invalid width.
Fractional Oneshot widths are converted before rounding (3.5us -> 28 counts).
Mode locking, CRC handling and prior-response DShot ordering remain unchanged.

`servo_source` links the real decoder, `src/io.c` and `src/util.c`, using section
garbage collection only for unrelated uncalled source functions. Host unsigned
char matches the ARM ABI. Hardware watchdog/reset boundaries are mocked.
`servo_trim_source` invokes the real shared callback with a mocked actuator.
Nine original strict calibration period windows, accumulated-error correction,
every-fourth capture, counter wrap and transition to Servo are tested.
Existing decoder/BiDShot regressions and all 11 CTests pass; default/selected
Application and Boot strict ARM links, six recovery negatives, image and
immutable-document audits pass with official GCC/SDK.

Calibration actuation is **BLOCKED** until a real, TI-supported clock provider
is supplied. Explicit selection `AM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED=1`
requires `am13e_app_servo_clock_trim(int)`; there is no weak/no-op implementation.
Without that provider, reception retains its previous uncalibrated behavior,
and `am13e_app_io_servo_calibration_blocked()` reports configured calibration
as unavailable. Mocked correction direction is not an on-target trim proof.
Original discovery capture counts, polarity re-selection and immediate handling
of incoherent/out-of-budget/over-2500us capture groups remain pending; those
groups are still rejected before policy and do not refresh the watchdog.

Service/handoff milestone CI: run 38154306786, exact HEAD
`d81d1d450618bb101f75a74a5b9e79e1fc1a0025`, job `fw1-build-evidence` SUCCESS,
all 22 reported steps SUCCESS. This does not establish full Rel17 parity.

## Reachable conditional CLI entry

Requirements APP-I01/I07/I24/I31, SYS-10:
**SOURCE IMPLEMENTED / SOFTWARE VERIFIED (policy + ARM links)**.
The existing explicitly selected CLI board capability now connects PB14's
HIGH timeout observations through the 16kHz input service to PendSV. The
production entry policy preserves Rel17's cumulative sixteen HIGH timeouts
(LOW does not reset source counter c); edges restart the timeout, and a
selected receiver or received UART byte cancels entry. Mode0 may initialize
the selected CLI UART without accepting binary commands in the wrong source
mode. CLI selects 38400/8N1 even after an SBUS parity/stop-bit configuration.
An occupied TX queue, pending CRSF command, active motor, busy UART or RX bytes
refuses takeover without a blocking idle wait. Masked capture takeover checks
receiver selection/BiDShot ownership again, stops ECAP0 and clears its pending
state; input calibration and callbacks stop while CLI owns service.

`cli_entry` tests the production policy's exact threshold/polarity, interrupted
timeouts, LOW observations and receiver-selection race cancellation. Existing
CLI editing/overflow/deferred-00ff tests remain. Selected CLI/CRSF and other
UART/telemetry ARM links, default Application/Boot and 11 CTests pass. These
tests do not emulate the entire UNICOMM register backend or prove physical
PB14 bias, entry timing, UART inversion, queue races or line topology.
**HARDWARE PENDING**: reviewed PB14 entry electrical condition and selected UART
pad/topology; measured entry/receiver arbitration. Default UART remains disabled.
Original single-wire CLI transport and complete discovery state machine remain
**DESIGN REQUIRED**, not closed by the conditional two-wire implementation.

Servo milestone CI: run 38154801219, exact HEAD
`a383147b2597ca54346b06151e9bdf50e27397f3`, job `fw1-build-evidence` SUCCESS,
all 22 reported steps SUCCESS.

## Conditional PARK/ERPM and shared motor traces

Requirements APP-M01/M04/M08/M17, APP-I18, SYS-10:
**SOURCE IMPLEMENTED / SOFTWARE VERIFIED** for original shared parking state
and ERPM source-event integration. `PARK_PIN`/`ERPM_PIN` remain original feature
selectors. Selection now requires separate explicit TI GPIO/mask/PINCM routing;
there is no default product pad assignment. Existing Reference owners (including
crystal pads), auxiliary reservations and selected UART/telemetry pins are
excluded. Mask/PINCM agreement is checked at compile time, port/PINCM agreement
before output enable, and initialization requires masked IRQs and power/mux
readback. PARK remains pull-up input; ERPM initializes low before output enable.

`park_source` includes actual `src/main.c`, executes its private `park()` and
`nextstep()` and retains the production six-step phase planner. It checks the
original 800-call transitions, stable HIGH/LOW sequence, reverse change,
measured return-duration counter, timeout and unstable-input refusal; motor
forward/reverse/coast/sine calls, delayed comparator selection and original
step1/step4 or sine step1/step181 ERPM levels. Physical motor/timer/comparator
and GPIO boundaries are mocked. It does not run the entire main control loop.
Wrong-port and UART-ownership conflicts halt before any output-enable call.
PARK-only, ERPM-only and combined synthetic profiles strictly ARM-link with
real native hook symbols. Five compile negatives cover missing routes,
overlap, reserved gate pad and mismatched mask. None of these fixtures is a
product board pin assignment or a flashable hardware-qualified image.

**HARDWARE PENDING**: actual optional pads, pull-up/electrical levels, ERPM load,
parking physical feedback and motor behavior. Hall hybrid IRQ/capture, discrete
LED/WS2812 and BEC native adapters remain **DESIGN REQUIRED / NOT IMPLEMENTED**.
The source-scope gate now also requires all added native host PASS groups and
the new production integration seams, while retaining every prior safety check.

Production-CRC milestone CI: run 38155597463, exact HEAD
`39d3c6e64aef90f0df8fa85104030cad96b6c689`, job `fw1-build-evidence` SUCCESS,
all 22 reported steps SUCCESS.

## Final local convergence replay

At firmware HEAD `20d3a10a9b19d466f98038c66d53791d9f176d92`, all 67 emitted
native Host PASS groups succeed (60 baseline groups plus seven additions).
Startup, smoke and full-image CTest suites each pass 11/11. Replayed gates:
image-profile positive/negative; all FW1 objects and historical strict diagnostic
link (configure/object/link return codes all zero); portable/backends/clock;
smoke and full flat images; Boot strict link; linked-image/layout;
Reference IO conflicts; selected UART/CRSF+CLI/EXBUS/telemetry ARM links;
functional coverage; original-document/ABI audit; strengthened source-scope
audit. Added PARK/ERPM profile links and five routing negatives pass. Six Boot
recovery-negative combinations pass. The fully recovery-qualified synthetic
positive Boot fixture is deliberately not enabled locally; existing CI retains
that fixture, which does not demonstrate hardware recovery.

PARK/ERPM milestone CI: run 38156533297, exact HEAD above,
job `fw1-build-evidence` SUCCESS, all 23 reported steps SUCCESS. All five
firmware/test milestones were published through normal non-force pushes with
matching successful CI; local platform branch remains `work`.

`CONVERGENCE_REQUIREMENTS.csv` covers all 73 original APP/BOOT/SYS IDs. It
retains clearly labeled historical audit evidence and lists current bounded
implementation/test deltas and remaining gaps. No row claims hardware
qualification. A source-scope/document PASS is traceability, not complete parity.
The pipeline still selects its Git SDK comparison and action-installed compiler;
official release provenance is established by the separate local validation.
Full legacy-platform builds and complete original command/main-loop regression
remain unexecuted. SDK-source compatibility corrections remain generated outside
the repository/installed SDK, as in the existing build infrastructure.

## Production Boot CRC framing coverage

Requirements BOOT-M01..M04, BOOT-I01/I02, B01/B02:
**SOFTWARE VERIFIED** for production CRC adapter configuration and wire framing.
All three existing protocol CTests now link actual `boot/src/util.c::crc32()`
instead of replacing it with a host CRC implementation. Only DriverLib CRCP
is mocked: power/init, 32-bit polynomial 0x04c11db7, reversed bit order,
little-endian input, no output-byte swap, all-one seed and byte feeding are
asserted. A separate MSB-first polynomial/bit-reversal boundary model is
checked against the existing reflected wire oracle, fixed CRC golden vectors
and every 4-byte quantum through 1024 bytes. Invalid pointer/length inputs
perform no modeled CRCP initialization. Existing malformed/CRC-failure parser
tests retain zero Flash-operation assertions and original ACK/NAK expectations.

Startup, smoke-image and full-image host suites each PASS 11/11; Application
and Boot official-toolchain strict links, layout/docs audits and six recovery
negatives PASS. **HARDWARE PENDING**: CRCP register behavior/result equivalence
on AM13E and captured host/target wire interoperability. The model is not a
verification of TI's actual peripheral. Production protocol/CRC code is unchanged.

CLI milestone CI: run 38155216282, exact HEAD
`ebcfe735072392449548e5373a87206437b59f01`, job `fw1-build-evidence` SUCCESS,
all 22 reported steps SUCCESS.

## Round 2: production command/configuration persistence

Starting HEAD: `f1dbbf10e6f824a447d8e1894d55784057ef3541`.
DShot Reset (47) previously called `resetcfg()` from capture IRQ and could
enter Bank0 configuration erase/program there. Like Save (12), it now queues
a coalesced operation for the existing PendSV service. An IRQ-masked claim
retains a pending request while BiDShot TX owns the line; nonzero throttle or
active motor cancels persistence. A running transaction refuses requeue.
Legacy DShot branches and original sixth-repeat/telemetry-bit policies are unchanged.

`config_command_source` links production `src/io.c`, unchanged `src/prog.c`,
real defaults/globals from `src/main.c`, actual `src/util.c` configuration
functions and native configuration plan/writer. Hardware cfg-commit boundary
is mocked; the real `cfg_flash_runtime.c` register/RAM closure is not exercised.
Tests execute deferred Reset/Save, first-five/sixth repetition, TX deferral,
throttle/motor cancellation, reentrant capture during save, invalid CRC/no
watchdog feed, wrong telemetry bit, original direction/3D throttle commands,
CLI parse/range errors and save/reset busy/motor refusal, serialized reload,
program/readback failure reporting, and actual CRSF Save/Reset confirmation.
No Flash operation occurs in capture context; unchanged persistence is wear-free.
This does not claim atomic storage or power-loss recovery: the native writer
still reports failure after an interrupted transaction, with no rollback.

All original-platform targets were compiled/linked using official GCC 15.2.1
and libopencm3 `ddcbb2889fa018626c604e27ea50e66db5755b78` (external checkout):
STM32F051/G071/G431/L431, AT32F421, GD32E230/F350 and eight original Boot
targets. No legacy MCU implementation or immutable design document was edited.
This is software compile/link evidence, not board qualification.

Round 2 milestone local validation: default startup 11/11 CTests; smoke and
full-image suites each 11/11; full native Host block including the added
source-linked regression; historical strict FW1 link; portable/backends/clock;
selected UART/CRSF+CLI/EXBUS/telemetry and PARK/ERPM profiles; five auxiliary
routing negatives; six Boot recovery negatives; image/layout, documentation,
functional coverage and source-scope audits; all 52 legacy ELF links PASS.
The added test fails against the baseline capture-context Reset implementation
at the hardware commit boundary, and passes after the fix. All physical
recovery/self-update/WRP operations remain gated and were not executed.

## Round 2: reachable selected native serial modes

First Round 2 milestone published as `f6c11b099bd3342ca64e1cb86ce318cd7ab682f0`,
matching GitHub Actions run 38160041985 / job 114530847737: SUCCESS, all 23 steps.

Production `checkcfg()` previously forced input modes 2..7 to zero because
AM13E has no legacy `IO_PA2`. A board-selected native serial RX capability
now retains the original 0..7 range and original DISABLE_EXBUS/HOTT checks
and channel limits. Analog mode 1 remains unavailable without ANALOG_CHAN;
no legacy pin alias or fabricated analog route is introduced. Unselected
Reference still validates to mode0. Default initialization, CLI/CRSF writes,
and restored configuration now reach the existing native UART selectors.

`receiver_config_default`, `receiver_config_serial`, `receiver_config_disabled`
execute real defaults and production checkcfg(), io callbacks and util throttle,
brake and CRC functions with the native serial/receiver decoders. Coverage:
mode 0..7 and disabled-capability validation, channel limits; binary serial
throttle; iBUS, SBUS, CRSF, EXBUS and SUMD throttle/brake/aux updates; actual
SBUS2 six-payload/delay-request generation and arbitrary end-byte admission;
malformed length, checksum failure and wrong-mode zero watchdog refresh.
The selected test fails against baseline checkcfg() and passes after the fix.
Only watchdog hardware is replaced in this receiver test; UART registers,
FIFO/interrupt arbitration and physical transport timing are not exercised.
SBUS2 still lacks actual timed TX/RX mute, and payload/delay tests do not close it.

Official default/selected Application and Boot links, 11 startup CTests,
smoke/full suites, complete native block, six recovery negatives, all existing
profile links/audits and all 52 legacy links PASS in the current milestone replay.
Original legacy checkcfg branches and immutable Rev1.4 documents remain intact.

## Round 2: conditional discrete LED production backend

Receiver milestone published as `0c6f4e55817b4296f1e8a36257d96725980d862c`,
matching GitHub Actions run 38160396948 / job 114531896510: SUCCESS, all 23 steps.

Native `aux_gpio.c` now provides original `initled()` / `ledctl(int)` hooks
when upstream LED_MAP selects 1..4 discrete push-pull LEDs. Every selected
LED requires independent TI GPIO/mask/PINCM definitions. The original count,
logical bit and LED1..4_INV policies are retained; global LED_INV applies to
the first three LEDs exactly as upstream. Default Reference selects no LED.
All routes validate before output setup; inactive latches and GPIO muxes are
established before any LED output enable. Initialization requires masked IRQs
and refuses repeated calls; ledctl refuses use before initialization. Native
reservations, GPIO/bit/PINCM agreement, selected serial/telemetry/analog,
PARK/ERPM and inter-LED conflicts are rejected. Auxiliary initialization also
rejects a LED-owned pin before enabling ERPM, regardless of initialization order.

`led_source`, `led_inverted_source`, `led_four_source` link actual main.c and
io.c with the native GPIO implementation, mocked TI/CMSIS boundaries only.
They execute real DShot LED commands, source PendSV 1kHz divider, status
fallback/config override and shutdown-before-fault-indication ordering. All
16 bit patterns, inactive-before-enable, individual/global inversion, 3/4
LED counts, stopped-motor command policy and repeated/uninitialized refusal
are tested. Wrong GPIO port, selected UART owner, LED/ERPM conflicts in both
initialization directions cause zero output-enable calls. Two nonflash ARM
profiles link the real hooks. Six compile negatives reject missing routes,
reserved gate pad, mismatched bit mask, duplicate routes, open-drain and WS2812.

**SOFTWARE INCOMPLETE**: open-drain and WS2812 native electrical/timer backends
remain absent and selection fails clearly rather than pretending push-pull
provides their semantics. **HARDWARE PENDING**: actual pad assignment, external
resistor/voltage/current/load and GPIO polarity/timing qualification. Synthetic
profiles never establish Reference wiring or physical applicability. Hall/BEC
remain unimplemented, and the LED work does not alter motor/recovery gates.

LED milestone local replay PASS: full native Host block (including seven LED
source/ownership groups), default startup and smoke/full CTests each 11/11,
Application/Boot/historical strict links, portable/backends/clock, selected
UART/CRSF+CLI/EXBUS/telemetry and PARK/ERPM links, five auxiliary and six Boot
safety negatives, layout/docs/functional/source-scope audits and 52 legacy
ELF links. Separate discrete 3/4-LED links and six LED compile negatives PASS.
Hardware outputs were not exercised. Existing safety checks remain mandatory.
