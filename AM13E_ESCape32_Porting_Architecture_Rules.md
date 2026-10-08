# Generic TI AM13E Port — ESCape32 Architecture Rules

**Status:** Mandatory porting rules  
**Branch:** `am13e-port-v2`  
**Baseline:** Original ESCape32 rel17 rev17 patch3 on `master`  
**Scope:** Generic AM13E MCU port, not a product-specific firmware fork.

## 1. Core principle

Follow the original ESCape32 **target naming, `add_target()` declaration style, source organization, firmware control flow, and native build conventions**. Add only the minimum AM13E platform-specific implementation needed. Any departure requires a documented technical reason and review.

The existing ESCape32 code is the reference implementation. A port must not grow into a parallel firmware architecture.

## 2. Target and naming contract

- Use `add_target(NAME MCU OPTIONS...)` in root and boot `CMakeLists.txt`.
- Preserve existing application and bootloader naming patterns, including explicit interface/pin options where relevant.
- Use `TI_AM13E` as the MCU platform selector, with a consistent `TI_AM13E` compile definition.
- Use generic MCU names such as `AM13E` and `BOOT_AM13E_PB14` rather than product-program identifiers.
- Application and bootloader targets must map to **their own correct source sets, linker configuration, and image semantics**. Shared declaration syntax does not justify using application sources to construct a bootloader.
- Preserve the existing targets and their configuration unchanged.

## 3. Conditional compilation contract

Use a two-path split only when peripheral/platform differences require it:

```c
#if defined(TI_AM13E)
    /* AM13E implementation */
#else
    /* Original ESCape32 implementation, unchanged */
#endif
```

Do not add a new layer of MCU-family-specific branches in previously shared legacy code. Existing legacy conditionals remain exactly where they are. Portable logic must not gain a platform conditional without a demonstrated requirement.

## 4. Firmware authority and dependency boundaries

- Preserve ESCape32 rel17 motor state transitions, commutation policy, BEMF timing policy, configuration/parameter semantics, and supported protocol behavior.
- Do not introduce a second motor-control loop, BEMF policy engine, runtime main, or parallel parameter authority.
- Use TI DriverLib / SDK / device documentation as the basis of AM13E peripheral code, without mechanically emulating STM32 registers.
- Keep clock, pinmux, comparator, MCPWM AQ/dead-band, eCAP, interrupt, flash, and trip-path code in the appropriate AM13E MCU backend.
- Avoid additional wrappers, host-test harnesses, or parallel build systems unless there is an independently reviewed necessity.

## 5. Board qualification and safety

A generic MCU port must not hard-code unverifiable board-specific values. In particular, do not infer:

- Clock frequencies or divider relationships
- Pin routing and comparator phase mapping
- Gate-driver polarity, enable/disable states, or dead-time
- MCPWM output/action qualifier and six-step commutation states
- Protection/trip behavior or watchdog assumptions
- Flash partitions or update guarantees

Keep motor outputs disabled until all required board-specific assumptions have been verified and protection paths qualified. Document each unresolved hardware dependency.

## 6. Source-wide port workflow

1. Inventory all relevant `.c`, `.h`, CMake, startup, linker, and generated SDK dependencies.
2. Port and review the **entire applicable source set** first, preserving original legacy behavior.
3. Audit platform-boundary leaks, duplicate control ownership, and incomplete application/bootloader build wiring.
4. Only then perform the first **native ESCape32 CMake build** for the AM13E targets.
5. Interpret failures and verification scope accurately: source review, object compilation, ELF linking, and board validation are distinct results.

## 7. Acceptance checklist

- [ ] Original `add_target()` naming and declaration conventions are preserved.
- [ ] `TI_AM13E` is the single added platform selector.
- [ ] No product-program identifier appears in generic port APIs, source modules, or build target names.
- [ ] Original STM32/AT32/GD32 code paths and target definitions are unchanged.
- [ ] Original ESCape32 rel17 control/policy logic remains authoritative.
- [ ] No independent runtime, motor policy, build wrapper, or fake MCU register compatibility layer has been added.
- [ ] All relevant sources and platform dependencies have been reviewed before native build.
- [ ] Board-specific uncertainty is explicitly identified and does not silently become a motor-enable default.
- [ ] Native build and hardware validation outcomes are reported separately.

## 8. Maintenance rule

Every new AM13E porting change must be checked against this contract before commit. The source of truth for software conventions remains the original ESCape32 implementation; this document records the agreed porting constraints rather than replacing upstream design.

**Reference note:** The earlier Porting Discussion and References documentation contains technical background and board-specific design discussion. Preserve that historical reference material separately; this generic architecture contract does not silently rewrite or supersede unreviewed hardware decisions.
