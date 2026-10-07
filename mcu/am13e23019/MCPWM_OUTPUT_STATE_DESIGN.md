# AM13E MCPWM Output-State Design Decision

Status: **split-input dead-band architecture selected for compile and bench
validation**. Production output enable remains blocked.

## Evidence from SDK 26.01.00.03

The MCPWM has pair-local Action Qualifier registers:

```text
PWM1_AQCTLA / PWM1_AQCTLAS
PWM1_AQCTLB / PWM1_AQCTLBS
PWM2_AQCTLA / PWM2_AQCTLAS
PWM2_AQCTLB / PWM2_AQCTLBS
PWM3_AQCTLA / PWM3_AQCTLAS
PWM3_AQCTLB / PWM3_AQCTLBS
```

and pair-local software-force fields for A and B.

The dead-band block is different: `DBCTL`, `DBRED`, and `DBFED` are
module-wide and therefore shared by all three phase pairs.

The important additional finding is that the dead-band block can select PWMA
and PWMB independently as RED/FED inputs. Therefore a post-dead-band per-pair
override is not required.

## Selected fixed dead-band topology

Use one MCPWM instance per motor with one fixed dead-band configuration shared
by all three phase pairs:

```text
AQ PWMA --> RED --> non-inverted --> OutA
AQ PWMB --> FED --> inverted     --> OutB
```

DriverLib configuration:

```text
RED input       = PWMA
FED input       = PWMB
RED polarity    = active high
FED polarity    = active low (inverted)
OutA swap       = off
OutB swap       = off
RED/FED delay   = same product dead-time value
```

The shared dead-band configuration is not changed during commutation. Only the
pair-local AQ/compare shadow state changes.

## Phase truth table

Assuming logical active-high gate-driver inputs:

| ESCape32 phase state | AQ PWMA | AQ PWMB | MCPWM OutA | MCPWM OutB |
| --- | --- | --- | --- | --- |
| PWM_POSITIVE, damp=1 | PWM | same PWM | PWM with delayed turn-on | complementary PWM with delayed turn-on |
| PWM_POSITIVE, damp=0 | PWM | HIGH | PWM with delayed turn-on | LOW |
| DRIVE_NEGATIVE | LOW | LOW | LOW | HIGH |
| FLOAT | LOW | HIGH | LOW | LOW |

This directly provides the three simultaneous six-step states required by one
motor:

```text
one pair PWM_POSITIVE
one pair DRIVE_NEGATIVE
one pair FLOAT
```

without changing module-wide dead-band settings.

## Why pair-local AQ software force is not the commutation mechanism

The SDK exposes:

```text
DL_MCPWM_setActionQualifierSWAction()
DL_MCPWM_AQ_SW_CONTINUOUS_LOW
DL_MCPWM_AQ_SW_CONTINUOUS_HIGH
```

for every A/B output.

However, `PWMx_AQSFRC` has no shadow register in the MCPWM register map.
Using six independent AQ software-force writes during a commutation transition
would make the phase update non-atomic.

Therefore normal ESCape32 six-step transitions will **not** use AQSFRC.

Instead, each logical phase state will be encoded into the pair-local
`AQCTLA/B` shadow registers. Compare values and all six AQ shadow tables can
then be committed together through MCPWM one-shot Global Load at the selected
time-base event.

AQ software force remains useful for compile verification, initialization, or
diagnostics, but not as the normal high-speed commutation state transition.

## PWM source convention

For the first bench implementation, use the same edge-aligned convention as
the TI basic PWM example:

```text
AQ source HIGH at CTR = ZERO
AQ source LOW  at CTR = CMP
```

For active freewheeling, the A and B AQ inputs use the same source waveform and
the fixed dead-band path produces complementary final outputs.

For non-damped PWM, B is represented by a constant-HIGH AQ table, which becomes
a constant-LOW final B output after the configured inversion.

## Remaining validation before output enable

1. Compile-check the fixed split-input dead-band DriverLib calls.
2. Encode LOW, HIGH, and PWM states using AQ shadow tables only.
3. Prove Global Load updates all three phase pairs at the intended boundary.
4. On hardware with power stage disabled/disconnected, measure:
   - A/B polarity;
   - RED and FED dead time;
   - PWM_POSITIVE with damp on/off;
   - DRIVE_NEGATIVE;
   - FLOAT;
   - all six commutation states.
5. Confirm final E62 gate-driver input polarity and MCPWM pin assignment.
6. Only then replace the compile-time stop in
   `hw_motor_apply_six_step()`.

Trip Zone remains the asynchronous protection path and is not used to encode
normal commutation states.
