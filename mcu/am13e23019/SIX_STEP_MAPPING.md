# AM13E23019 Six-Step Output Mapping

Status: semantic mapping defined; hardware gate-state implementation not yet
enabled.

This document converts the existing ESCape32 six-step code into a
platform-neutral phase-state contract before the AM13 MCPWM implementation is
allowed to drive outputs.

## ESCape32 commutation sequence

The rel17 control path defines the following six electrical steps:

| Step | Positive phase | Negative phase | Floating phase |
| ---: | :---: | :---: | :---: |
| 1 | C | B | A |
| 2 | A | B | C |
| 3 | A | C | B |
| 4 | B | C | A |
| 5 | B | A | C |
| 6 | C | A | B |

This is the semantic behavior already encoded by `nextstep()`. Reverse
rotation changes the step order; it does not require a second hardware truth
table.

## Logical phase states

For the AM13 backend, each phase must be treated as one of these logical states:

```text
PWM_POSITIVE
    High-side follows the commanded PWM.
    If active freewheeling/damp is enabled, the low-side is complementary
    during the high-side off interval, with dead time.

DRIVE_NEGATIVE
    Low-side is continuously driven on.
    High-side is off.

FLOAT
    High-side off.
    Low-side off.
```

When `throt_ztc` is enabled and throttle is zero, ESCape32 explicitly sets the
positive and negative masks to zero. Therefore all three phases must resolve to
`FLOAT`; this is the zero-throttle coasting contract.

The configuration parameter `damp` is ESCape32's complementary-PWM / active
freewheeling mode. It only changes the `PWM_POSITIVE` phase behavior; it must
not turn the nominally floating phase into a conducting phase.

## AM13 MCPWM resources

One AM13 MCPWM instance provides three A/B channel pairs:

```text
Phase A -> PWM1A / PWM1B
Phase B -> PWM2A / PWM2B
Phase C -> PWM3A / PWM3B
```

TI's SDK examples confirm:

- compare values have shadow registers;
- Action Qualifier A/B actions have shadow registers;
- dead-band supports complementary upper/lower-switch relationships;
- global load can commit compare, AQ and dead-band updates synchronously;
- Trip Zone is downstream of dead-band and can force A/B outputs low for
  asynchronous protection.

The intended ESCape32 update sequence is therefore:

```text
compute next six-step phase states
        |
        +-- write compare shadow values
        +-- write AQ shadow state
        +-- prepare dead-band behavior as required
        |
        +-- one-shot global-load commit
        |
        +-- hardware applies the new state at the configured load event
```

## Important hardware constraint

Do not equate logical HIGH/LOW in this document with MOSFET gate ON/OFF until
the E62 gate-driver input polarity and final pin assignment are confirmed.

The production backend must define the product-level mapping:

```text
MCPWMx_yA -> phase high-side gate-driver input
MCPWMx_yB -> phase low-side gate-driver input
logical active level -> physical gate ON
```

The TI LaunchPad example pin assignment is a compile/reference fixture only. It
is not an E62 schematic or pinout.

## Why the old STM32 register pattern is not copied

The existing libopencm3 backend uses TIM1 CCER/CCMR modes and complementary
outputs to realize these states. Those register encodings are implementation
details, not the portable contract.

The AM13 backend must implement the three logical phase states directly using
native MCPWM AQ/dead-band/global-load behavior. It must not emulate TIM1
CCER/CCMR.

## Stage-C acceptance criteria

Before `hw_motor_apply_six_step()` is enabled on AM13:

1. the TI AQ shadow-update API must compile for all six MCPWM outputs;
2. the exact dead-band mode for active freewheeling must be selected;
3. a per-phase way to obtain `FLOAT` (both gate commands inactive) while
   another phase is `DRIVE_NEGATIVE` must be proven;
4. global-load timing must apply commutation state changes without transient
   invalid combinations;
5. gate-driver polarity/pin mapping must be a product configuration input, not
   inferred from the TI LaunchPad example;
6. the first hardware test must be gate-driver disconnected or power-stage
   disabled and verified on a scope/logic analyzer before driving a motor.

Until these conditions are met, the AM13 backend intentionally leaves
`hw_motor_apply_six_step()`, output enable, and idle-state mapping as
compile-time stops.
