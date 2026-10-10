# AM13E Editable Board Configuration

**Software development default:** the reference FW1 stays fail-closed; no
verified gate, dead-band, analog calibration or independent OC wiring is
inferred. Board-dependent values live in `board_configuration.h` and use
`#ifndef` so a reviewed Board Profile may override each field individually.

| Field | Reference build software default | Later board override |
| --- | --- | --- |
| Pin Mapping | Existing PA8/11/9/30/10/31 MCPWM0, PB14 DShot, PB15 nFAULT, PA6 NTC, PA28 VBUS, PB13 reserved | Select another Board Provider; do not silently change reference assignment |
| Gate Polarity | PB13 active level `-1`, PWM invert `-1`, gate Hi-Z-safe and HW shutdown `0` | Verified Gate Driver/Schematic values |
| Dead-time | RED/FED `0` ticks, inversion, input topology, output swaps, offset `-1` | Verified non-overlap calculation and measured topology |
| Sensing | 12-bit code fullscale `4095`; analog VREF, supply, resistors, NTC model `0` | Verified schematic and sensor calibration |
| Fault Routing | Reference PB15 active-low nFAULT -> OST1 unchanged; independent OC pin `0` and polarity `-1` | Verified separate OC source -> OST2 |

`-1` is **unknown**, NOT a voltage or logic level. `0` is
**unconfigured**, NOT a safe physical dead-time or sensor value. Neither
convention authorizes hardware. Existing macros
`AM13E_BOARD_POWER_STAGE_PROFILE`,
`AM13E_MOTOR_BOARD_DEADBAND_VERIFIED` and
`AM13E_BOARD_SENSORS_CALIBRATED` remain **undefined by default**.
Opting in with the default unknown values fails compile-time assertions.
The default therefore compiles software and Boot/FW1 images while the
physical power stage stays disconnected and gate enable is untouched.

For later hardware qualification, edit these macros via a reviewed
`AM13E_BOARD_BOARD_PROFILE_FILE` (CMake) or explicit `-D` definitions.
Do not mistake the synthetic compiler fixtures in
`tests/board_profile_compile_smoke.py` for actual board parameters.
The existing Reference Pin Mapping has its own strict assertions and
a different PCB must supply a new explicit Board Provider.
