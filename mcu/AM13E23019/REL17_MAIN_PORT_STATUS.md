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
