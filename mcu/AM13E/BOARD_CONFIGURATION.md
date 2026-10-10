# AM13E Board Configuration — IO-only auxiliary features

**Scope update:** The five auxiliary functions are **IO initialization
ONLY**. They are not functional and are not part of the AM13E motor,
telemetry or protection runtime. This explicitly replaces the earlier
proposal to enable the PB13 gate driver and nFAULT/OC fault trip.

| Function | Default pin and initialization | Explicitly excluded |
| --- | --- | --- |
| Gate Driver Enable | PB13 / GPIO1 output, initialized to the defined **INACTIVE** polarity and held there | No enabling/disabling driver on motor requests, no PWM pad attach |
| Driver nFAULT | PB15 / GPIO1 input, GPIO interrupt masked | No SysTick polling, GPIO ISR fault action, MCPWM OST1 mapping |
| Independent OC Trip | `AM13E_BOARD_OC_GPIO_PINCM=0` = unassigned; if specified, configure GPIO input only | No INPUTXBAR/PWMXBAR/OST2, OC interrupt or shutdown |
| Serial Telemetry TX | `AM13E_BOARD_SERIAL_TX_PINCM=0` = unassigned; if specified, keep GPIO input/Hi-Z | No UART mux, TX DMA, serial telemetry transport |
| Current Limiting | `AM13E_BOARD_CURRENT_SENSE_PINCM=0` = unassigned; if specified, configure analog pinmux only | No extra ADC SOC, conversion, current sensing, limit PID or overcurrent protection |

`0` denotes **unassigned optional pin**, not a real pin. The three
optional pins cannot share a PINCM or overlap any active Reference motor,
BEMF, DShot or ADC route; static compile assertions reject collisions.

## Still functional in the firmware

Full Rel17 application algorithms remain compiled and linked:
six-step/sine, PWM timer & compare/AQ staging, adjustable RED/FED
dead-band, BEMF CMPSS/ECAP, DShot / BiDShot command & reply, NTC/VBUS
ADC0 sampling & numerical model, Motor Music/PCM using **the same
MCPWM0**, flash configuration, watchdog policy, the active Rel17 v1.4 Cfg.id/vector Boot contract.

The G431-derived software defaults are still editable: RED/FED
`92/92 ticks` at 100MHz, NTC model 3, nominal VREF 3300mV and
VBUS divider 26820:1000. These are development values, not PCB
measurement data. No current ADC channel has been invented.

**Important limitation:** Six motor MCPWM0 outputs remain internally
operational but their GPIO pads remain disconnected, and PB13 remains
INACTIVE. Consequently this Reference firmware is **not motor-spinning
or Gate Driver enabled**; pretending otherwise would contradict the
requested "IO only, no functional implementation" scope.

The FW1 CMake option is `AM13E_MOTOR_RUNTIME_ENABLED=ON`.
It builds real motor PWM/dead-band and ADC software paths, **not an
enabled physical power stage**. `AM13E_POWER_STAGE_ENABLED` is retired.

## Sources and CI gates

- `fault_input.c`: PB15 input plus optional OC, UART TX and current
  sense pin-mode preparation. No IRQ enable.
- `motor_power_stage.c`: PB13 inactive-output initialization only.
  No attach/enable functions exist in this module.
- `motor_safety.c`: pure MCU PWM scheduling, no gate attachment or
  nFAULT/OC runtime dependency. Motor Audio still owns MCPWM0.
- `irq_vectors.c`: ordinary SysTick, no nFAULT fault processing.
- `motor_fault_route.c` and generic `fault_trip_backend.c` remain
  in source tree for future design work; neither is linked into
  Reference FW1. The independent generic backend is still compiled
  in CI to retain its software regression coverage.

CI verifies syntax of the optional IO reservation paths, pin-conflict
negative cases, and absence of operational gate/fault integration in
the actual FW1. It also verifies strict Rel17 APP/Boot link,
native motor/ADC regression, Rel17 v1.4 flat-image/Boot host tests and linked-size bounds.
