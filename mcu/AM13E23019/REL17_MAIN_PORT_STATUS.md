# E62 / AM13E23019 — rel17 main.c port status

## Entry-point decision

- **Authoritative firmware entry:** `src/main.c` from ESCape32 rel17.
- `am13e_runtime.c` is **not included** in the E62 executable. It remains in the tree only as a reference to previous hardware bring-up.
- `am13e_prog_port.c` is **not included**: `cfg` and other rel17 firmware globals must be owned by `src/main.c`, not by a parallel bring-up adapter.
- Shared configuration, parameter access, CLI/CRSF command parsing, BEMF math and six-step policy remain reused.
- No E62 firmware build success is claimed for this migration commit. **The direct rel17 MCU port is presently blocked by actual STM32 peripheral dependencies.**

## Known migration blockers (do not stub)

1. `src/main.c` directly accesses STM32 timer registers for BEMF capture / compare, dead-time and TIM1 commutation IRQ paths (`TIM_ARR`, `TIM_EGR`, `TIM_CR1`, `TIM1_*`, `IFTIM_*`).
2. `CLK`, `DEAD_TIME`, `COMP_MAP` and related product-specific settings require confirmed AM13E/E62 sources, rather than copied STM32 values.
3. Complete PWM, ADC, comparator, interrupts, throttle input, watchdog, fault response, gate-driver enable and physical pinmux contracts must be implemented using AM13E DriverLib and verified against board requirements.
4. `src/main.c` references further target hardware symbols and system initialization that must be migrated without replacing motor control with no-op operations.
5. `prog.c` is linked, but command transport, persistence, and safety policy are not yet validated as an integrated firmware path.

## Acceptance criteria

- E62 only has **one** `main()`, from original `src/main.c`.
- Build and linker map prove the actual rel17 firmware control loop and ISR dependencies, not the legacy bring-up loop.
- Hardware timing and protection constants are traced to board-level requirements or AM13E documentation.
- Host regressions remain passing.
- Motor phase output stays disabled until valid startup conditions and gate-driver safety are established.

Do **not** use the existing bring-up image as evidence of rel17 main.c integration. Existing E62 binary hashes refer to the earlier bring-up target.

## BEMF state ownership (migration in progress)

- The authoritative rel17 state variables are `ival`, `ertm`, `sync`, `fast` in `src/main.c`.
- `src/hw_bemf_rel17_bridge.h` translates **that same state** via `hw_bemf_rel17_process()` into arm/cancel timer intents; original `src/main.c::iftim_isr()` now consumes this result without changing STM32 timer hardware operations. `tools/am13e/run-rel17-bemf-bridge-host-test.sh` tests the portable contract and saves the output under `build-am13e/logs/`.
- The older `am13e_bemf_events.c` maintains **separate** `engine.policy` state for the diagnostic bring-up path. **Do not attach it directly to the rel17 main control loop.** Its eCAP/TIMG backend must instead consume rel17 bridge actions, with qualified timestamp epochs and clock domains.
- The bridge has **not** been connected to AM13E IRQs or peripheral registers. Main firmware cross-build remains blocked by documented clock, comparator and motor safety constraints.
- Build logs: `build-am13e/logs/e62-rel17-main-build.log` (do not use `/tmp`).

## October rel17 BEMF hardware I/O cutover

- E62 build now includes `am13e_bemf_io.c` rather than the bring-up `am13e_bemf_events.c` event engine. No duplicate BEMF policy state is linked from this path.
- `am13e_irq.c` and `am13e_timg12.c` dispatch eCAP0/TIMG12 events to this hardware-only I/O. All callbacks are **unbound** until the original `src/main.c` registers its rel17 capture and commutation handlers.
- `am13e_bemf_bind()` is a declaration of callbacks **only**, not a hardware-ready signal. It does not configure/enable eCAP, TIMG12 or NVIC.
- Actual rel17 callback binding and capture-unit conversion remain pending validated comparator source, eCAP epoch and clock relationship. The real E62 firmware remains compile-blocked by `CLK`, `DEAD_TIME`, `COMP_MAP`, STM32 register accesses, and safe output-state mapping.
- Do not interpret IRQ code compilation or host BEMF tests as proof of motor control integration.

## Integrated motor-control contract (four concurrent workstreams)

1. **MCPWM/commutation:** `hw_motor_am13e_commutation.h` validates six-step phase masks; `hw_am13e_safety_contract.h` adds output-qualification and arming gates. Real AQ / dead-band, gate-driver polarity and trip response are **NOT qualified**. Existing `_Static_assert` blocks remain.
2. **BEMF / eCAP / TIMG12:** `am13e_bemf_io.c` binds only when the qualification contract is satisfied and converts eCAP ticks to TIMG12 ticks with round-up and overflow rejection. Binding is still **not called from `main.c`** and no comparator source or IRQ enable is implied.
3. **Clock contract:** individual core, MCPWM, eCAP and TIMG12 frequencies are required fields, not an invented single `CLK`. Clock and dead-time values in host tests are **fixtures only** and must not be treated as E62 board settings.
4. **Fault and recovery:** pure-C fault latch, zero-throttle interlock and explicit clear/re-arm gate are implemented and host-testable. The real MCPWM trip input, watchdog action, hardware output shutdown and reset-cause mapping are **not wired**; therefore this is **not** a production-safe hardware fault path.

Host regression: `bash tools/am13e/run-am13e-safety-contract-host-test.sh` saves `build-am13e/logs/e62-am13e-safety-contract-host.log`. Host PASS does not replace E62 target Cross-Compile, board-level measurements or physical motor safety validation.

**Porting status: all four tracks active; integrated firmware and hardware validation incomplete.**

## Direct main.c BEMF ISR migration

- Under `ESCAPE32_AM13E`, the STM32-specific `iftim_isr()` is excluded and replaced by `am13e_bemf_capture()` callback processing the **original** `ival / ertm / sync / fast` state, then calling `am13e_bemf_arm_delay()` for TIMG12. The delay-expiry callback calls original `nextstep()`.
- eCAP ticks are explicitly converted to 1-MHz BEMF policy units and back (overflow checked), then the I/O layer converts to qualified TIMG12 ticks. No equality of unrelated clock domains is assumed.
- `am13e_bemf_connect()` is intentionally not called from `main()` until a verified board contract exists; **thus this is a compiled code path, not a working motor-control loop**.
- Nextstep/startup/duty/fault code still contains STM32 register access; the build remains blocked. AQ, dead-band and trip path assertions must not be disabled to force a build.

## Source-wide MCU isolation pass

- Legacy libopencm3 MCU implementations `src/io.c`, `src/telem.c`, and `src/util.c` now use a file-level `#if defined(ESCAPE32_AM13E)` / `#else` split. The AM13E branch provides **no fake function definitions**; original legacy sources remain unchanged in `#else`.
- `src/hw_platform.h` and `src/hw_motor.h` already choose independent AM13E vs legacy backends.
- `src/main.c` and `src/prog.c` remain mixed, with platform-specific sections gated. Portable `esc_*.c` remains intentionally common to all MCU builds.
- Run `python3 tools/am13e/audit-source-isolation.py` in the full checkout. It enumerates **all actual src/*.c files**, flags unknown files instead of assuming the GitHub single-file connector delivered a complete tree.
- This is source ownership isolation, not completion of the AM13E board I/O, watchdog, trip, PWM, pin mapping, or firmware linker integration.
