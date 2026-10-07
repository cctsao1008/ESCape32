# AM13E MCPWM Output-State Design Decision

Status: architecture constraint identified; per-phase output override mechanism
still under validation.

## Result from Stage-B / Stage-C probes

The following native TI APIs are already compile-validated against SDK
26.01.00.03:

```text
DL_MCPWM_setTimeBasePeriodShadow
DL_MCPWM_setCounterCompareShadowValue
DL_MCPWM_setActionQualifierActionShadow
DL_MCPWM_setGlobalLoadOneShotLatch
```

This is enough to prove that ESCape32 duty/AQ changes can be staged in shadow
registers and committed synchronously.

It is **not** enough to implement six-step commutation safely.

## Important AM13 MCPWM constraint

AM13 MCPWM exposes three PWM pairs per instance, but several resources are
shared across all three pairs. The TRM explicitly identifies at least these
shared settings:

```text
TBPRD
TBPHS
DBRED
DBFED
```

The register map also has only one module-level `DBCTL`, `DBRED`, and
`DBFED` set for the whole six-channel MCPWM instance.

That matters because ESCape32 six-step operation needs three different phase
states at the same instant:

```text
one phase: PWM_POSITIVE
one phase: DRIVE_NEGATIVE
one phase: FLOAT
```

With active freewheeling enabled, `PWM_POSITIVE` additionally needs a
complementary low-side waveform with dead time.

A classical module-wide complementary dead-band mode cannot be accepted as the
production mapping until we prove that the floating pair can still be forced to
both outputs inactive independently of the other two pairs.

## Candidate implementation paths

### Path A — one MCPWM instance, AQ-only per pair

Keep the dead-band block bypassed and drive A/B independently with each pair's
AQ/compare resources.

Advantages:

- one MCPWM instance per motor;
- per-pair PWM, low-side-on, and float states remain independent;
- compatible with a four-motor / four-MCPWM architecture.

Open issue:

- dead time must be synthesized with AQ/compare timing rather than the shared
  dead-band block;
- the resulting edge timing must be proven against ESCape32's existing
  edge-aligned behavior.

### Path B — one MCPWM instance, shared dead-band plus per-pair post-DB override

Use the hardware dead-band generator for complementary switching and find a
native per-pair override after the dead-band stage for `FLOAT` and forced
low-side states.

This would be preferred if the hardware provides such an override because it
retains hardware dead-time generation.

Open issue:

- the currently captured SDK reference does not yet establish a per-pair
  post-dead-band software override;
- Trip Zone is downstream of dead-band, but its A/B action is common across the
  module, so it cannot by itself represent one floating phase during normal
  commutation.

### Path C — multiple MCPWM instances per motor

Assign one MCPWM instance per phase so each phase has independent dead-band
configuration.

This is architecturally simple for one motor but does not scale to a 4-in-1
design on a device with five MCPWM instances, so it is not the preferred
product architecture.

## Current decision

Do **not** enable `hw_motor_apply_six_step()` yet.

The next question is narrowly defined:

> Does AM13 DriverLib expose a per-channel/pair software-force or override
> mechanism that gives the required final output state independently of the
> shared dead-band block?

If yes, Path B remains viable.

If no, move to Path A and synthesize the dead-time relationship using the
independent A/B AQ/compare resources.

## Evidence still required

Capture the exact DriverLib declarations and comments for:

```text
AQ software force
AQ one-time software force
continuous force / override
dead-band configuration
AQ shadow configuration
```

Run:

```bash
./tools/am13e/collect-mcpwm-force-reference.sh
```

The result is written to:

```text
.am13e-build/mcpwm-force-reference.txt
```

No power-stage test should be attempted until the per-phase FLOAT path is
resolved and represented explicitly in the backend.
