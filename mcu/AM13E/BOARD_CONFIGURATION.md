# AM13E Board Configuration — G431-derived Software Defaults

**All configurable fields have concrete numeric development defaults.**
These values are based where possible on ESCape32 Rel17
`PHOTONDRIVE1 STM32G431`, not a verified AM13E PCB schematic. Unlike
`-1` unknown placeholders, the numeric defaults can be used for source,
formula and simulation work; **physical motor output is still disabled**.

## G431 source baseline

`CMakeLists.txt` defines `PHOTONDRIVE1 STM32G431 DEAD_TIME=155
COMP_MAP=132 SENS_MAP=0xBFA6 VOLT_MUL=224 CURR_MUL=63
TEMP_FUNC=NTC10K3455UP10K`.

STM32G431 `src/defs.h` converts `DEAD_TIME=155` into `TIM_DTG=0x8D`:
effective `154/168MHz = 916.7ns`. Since AM13E MCPWM uses 100MHz,
we round to `92` ticks (920ns) for each RED/FED and the corresponding
compare-offset modelling. This is **time equivalence**, not a proven
equivalent six-output MCPWM dead-band topology.

| Board setting | Development default | Source / limitation |
| --- | --- | --- |
| Pin Mapping | PA8/PA11, PA9/PA30, PA10/PA31 motor; PB14 command; PB15 nFAULT; PB13 reserved; PA6/PA28 sensing | Existing reference routes preserved |
| Gate polarity | PB13 active `1`; invert `0`; Hi-Z safe proof `0`; hardware shutdown proof `0` | Active-high development convention only; PHOTONDRIVE1 does **not** verify PB13 polarity |
| Dead-time | RED/FED `92/92` ticks; offset `92`; polarity `0/1`; both inputs PWMA (`0`), no swap | G431-derived duration and conventional complementary output, not qualified analog driver behavior |
| Sensing | 12-bit `4095`; nominal VREF `3300mV`; NTC supply `3300mV`; Rtop `26820`Ω / Rbottom `1000`Ω; model `3` | Approximation from G431 `VOLT_MUL=224`, calibrated 3.3V model and `NTC10K3455UP10K`; NOT actual resistor/VREF data |
| Fault Routing | PB15 active-low -> OST1 unchanged; dedicated OST2 OC disabled: pin `0`, polarity `1` (inert) | G431 `COMP_MAP=132` handles BEMF, not overcurrent; no fake physical OC pin |

## Safety, build and future board revisions

The numeric values remain `#ifndef`-overridable from CMake's
`AM13E_BOARD_BOARD_PROFILE_FILE` or direct compile definitions.

**Defaults do not define** `AM13E_BOARD_POWER_STAGE_PROFILE`,
`AM13E_MOTOR_BOARD_DEADBAND_VERIFIED` or
`AM13E_BOARD_SENSORS_CALIBRATED`. The gate remains disconnected and
uncalibrated ADC readings do not become real voltage/temperature protection.
Explicitly forcing the power-stage configuration with only these development
defaults is rejected because independent OC, verified gate Hi-Z safety, and
hardware shutdown are unavailable. No GPIO routing, Boot/Image ABI or Motor
Audio resource ownership changes in this update.

The numeric placeholders are development assumptions, NOT design evidence.
Before any live test the real schematic, protection circuit, voltage divider,
gate-driver polarity and non-overlap waveforms must be checked and the Board
Profile updated as required. See `tests/board_profile_compile_smoke.py`
for synthetic compiler coverage, which is intentionally not production data.
