# AM13E23019 Commutation Boundary Analysis

Status: six-step AQ shadow encoding is compile-valid, but the commutation
activation boundary is not yet mapped.

## Existing ESCape32 / STM32 behavior

The rel17 firmware does not switch six-step states at the PWM period boundary.

TIM1 is configured with:

```text
CCPC = 1  -> CCxE / CCxNE / OCxM control bits are preloaded
CCUS = 1  -> a rising edge on TIM1 TRGI generates a COM event
TIM1 TS    -> ITR1
TIM2 TRGO  -> OC3REF
```

The BEMF timing timer therefore supplies a delayed hardware trigger to TIM1.
At the COM event, the preloaded output-control state becomes active and the COM
interrupt runs. The ISR then prepares the following commutation state.

Conceptually:

```text
BEMF zero cross
      |
      v
delay timer
      |
      v
TIM2 TRGO rising edge
      |
      v
TIM1 COM event
      |----------------------------+
      |                            |
      v                            v
preloaded gate state -> active     COM IRQ
                                    |
                                    v
                              prepare next state
```

This matters because the commutation instant is asynchronous to the PWM
time-base counter.

## AM13 MCPWM Global Load is not a drop-in COM replacement

AM13 Global Load can transfer shadow registers together, but the TRM states that
when `GLDCTL.GLD = 1`, the local shadow-load selection for individual
registers is ignored and the selected Global Load event applies to **all**
registers that have a corresponding shadow register.

That includes more than the six AQ tables. It also includes period, compare and
dead-band shadow state.

The available global-load event choices are:

```text
CTR = ZERO
CTR = PRD
CTR = ZERO or PRD
software force (GLDOSHTCTL.GFRCLD)
```

The TI global-load example used by the current compile probe enables one-shot
Global Load and, unless overridden in SysConfig, uses the counter-boundary load
event. That is suitable for glitch-free PWM parameter updates but is not
equivalent to ESCape32's asynchronous TIM1 COM event.

## Why PWM-boundary commutation is rejected

Quantizing each commutation to the next PWM boundary changes the ESCape32 timing
model.

At the current ESCape32 16--24 kHz PWM range, one PWM period is roughly:

```text
16 kHz -> 62.5 us
24 kHz -> 41.7 us
```

The added phase delay would vary with where the requested commutation instant
lands inside the PWM period. At high ERPM this can become a substantial fraction
of a six-step interval.

Therefore the initial AM13 port must preserve an asynchronous commutation
boundary rather than silently snapping commutation to CTR=ZERO/PRD.

## Closest AM13 primitive: software-forced Global Load

`GLDOSHTCTL.GFRCLD` can force a one-shot Global Load in software. This is the
closest MCPWM primitive to STM32 `TIM_EGR.COMG` because it can activate all
prepared AQ shadows at one defined software event.

However, Global Load also transfers pending period/compare/dead-band shadows.
ESCape32 currently treats these as two different timing domains:

```text
PWM duty / period updates
    -> normal PWM update boundary

six-step output-control updates
    -> BEMF-derived commutation boundary
```

Collapsing both domains onto one software-forced Global Load can update duty or
period in the middle of a PWM cycle. That is not accepted without a specific
coherency mechanism.

## Candidate AM13 commutation mechanisms

### A. Software-forced Global Load with shadow coherency

At the commutation ISR:

1. ensure any globally-loaded non-AQ shadows represent the currently active
   PWM state;
2. prepare all six AQ shadow tables;
3. arm one-shot Global Load;
4. force `GFRCLD`;
5. restore pending duty/period shadows for their normal PWM-boundary update.

This preserves atomic six-channel AQ activation, but it adds bookkeeping and
must be proven race-free.

### B. Safe-output mask plus direct AQ active update

Use a module-wide safe state (for example a validated Trip Zone path), update
the six active AQ control registers while outputs are masked, then release the
safe state.

This avoids Global Load coupling to compare/period shadows, but creates an
intentional commutation blanking interval and requires proof that trip release
and dead-band behavior are safe and deterministic.

### C. DMA-driven active AQ update

Use a timer/capture event to launch a prebuilt table of active-register writes.

This could reduce CPU latency and jitter, but the six AQ registers are not one
contiguous block and the update is still not truly atomic unless the output is
masked or hardware provides an additional synchronization mechanism.

### D. Load AQ at PWM ZERO/PRD

Rejected for the first faithful ESCape32 port because it changes commutation
timing as described above.

## Current decision

Do not connect `hw_motor_am13e_encode_six_step_shadow()` to
`hw_motor_apply_six_step()` yet.

Also do not treat the existing
`DL_MCPWM_setGlobalLoadOneShotLatch()` call as the production equivalent of
STM32 COM. It only arms the one-shot latch; the actual activation event still
depends on `GLDCTL.GLDMODE`.

The next implementation decision requires the exact pinned-SDK DriverLib
functions for:

```text
global-load enable/disable
global-load trigger selection
one-shot mode
one-shot latch
software-forced global load (GFRCLD)
one-shot latch status/clear
Trip Zone software force/clear, if exposed
```

Once those APIs are captured, Path A and Path B can be compared with real code
rather than register-name inference.
