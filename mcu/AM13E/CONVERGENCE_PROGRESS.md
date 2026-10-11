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
sequence; reachable HIGH-level CLI entry; SBUS2 scheduler; original
half-duplex transports; optional Hall/LED/BEC/PARK/ERPM hooks; broader shared
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
