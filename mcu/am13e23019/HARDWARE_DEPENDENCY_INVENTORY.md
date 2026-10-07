# ESCape32 AM13E23019 Hardware Dependency Inventory

Status: initial inventory for the `am13e-port` branch.

Purpose: identify where the current ESCape32 code is coupled to the
libopencm3/STM32 hardware model before introducing an AM13E backend.

This document is intentionally an inventory and boundary proposal. It does not
change motor-control behavior and it does not define a general-purpose HAL.

## Architectural conclusion

The current firmware is not merely linked against libopencm3. Several common
sources directly use STM32 timer, DMA, UART, flash, watchdog, GPIO and Cortex-M
register semantics.

For AM13E the intended architecture is:

```text
                 ESCape32 control / protocol
                           |
                 ESCape32 HW contract
                           |
              +------------+------------+
              |                         |
       existing MCU backend       AM13E23019 backend
              |                         |
          libopencm3              TI DriverLib
                                 SysConfig
                                 CMSIS
                                 MCPWM / CMPSS /
                                 ADC / eCAP / DMA
```

The AM13E backend must not emulate STM32 registers and must not require a port
of libopencm3.

## Existing coupling points

### `src/common.h`

`common.h` currently makes libopencm3 a transitive dependency of the portable
source tree by including Cortex-M3 and STM32 headers directly:

```text
libopencmsis/core_cm3.h
libopencm3/stm32/syscfg.h
libopencm3/stm32/rcc.h
libopencm3/stm32/gpio.h
libopencm3/stm32/timer.h
libopencm3/stm32/usart.h
libopencm3/stm32/dma.h
libopencm3/stm32/flash.h
libopencm3/stm32/iwdg.h
libopencm3/stm32/wwdg.h
```

This is the first include-boundary that must eventually be split.

### `src/main.c`

This is the highest-risk coupling because the main commutation and motor timing
path directly manipulates STM32 timers.

Major hardware-coupled regions:

| Function / region | Current dependency | Required AM13 semantic |
| --- | --- | --- |
| `nextstep()` (~117-329) | TIM1, IFTIM, CC/COM/update semantics | apply commutation state, PWM duty/update, arm zero-cross timing |
| `laststep()` (~331-353) | TIM1 CCMR/EGR COM event | finalize commutation transition |
| `tim1_com_isr()` (~355-375) | TIM1 COM interrupt and forced-output modes | commutation-update completion/event |
| `iftim_isr()` (~377-399) | timer capture/status/timeout | BEMF zero-cross capture and timeout |
| `tim3_isr()` (~402-420) | hall/capture timer + COM trigger | hall edge timing / commutation trigger |
| `sys_tick_handler()` (~464-468) | SCB PendSV/SLEEP control | low-priority scheduler kick |
| `delay()` (~470-476) | global IRQ mask | bounded critical section |
| `hard_fault_handler()` (~487-498) | TIM1 break, delay timer, WWDG | safe motor shutdown + delayed reset |
| `main()` (~558-868) | TIM1/IFTIM/TIM3/NVIC/reset flags/watchdog | platform initialization and runtime timing |

The first AM13 motor backend should therefore target the semantic behavior of
`nextstep()`/`laststep()`, not individual STM32 registers.

### `src/io.c`

This file has the broadest peripheral coupling. It combines protocol logic with
timer capture, DMA, UART, GPIO and watchdog operations.

Major coupled regions:

| Function / region | Current dependency | Required AM13 semantic |
| --- | --- | --- |
| `initio()` (~83-111) | timer input capture | initialize throttle/DShot input capture |
| `entryirq()` (~113-258) | capture timer, GPIO pin mode, UART, timeout | input-mode detection / entry state machine |
| `calibirq()` / `servoirq()` | capture timer + watchdog | servo/oneshot pulse measurement |
| `dshotirq()` (~305-322) | timer update + DMA count | DShot frame completion / malformed-frame handling |
| `dshotreset()`, `dshotresync()` | timer reset/reconfigure + DMA | DShot capture resynchronization |
| `iotim_dma_isr()` (~363-549) | timer/DMA direction changes | DShot/BiDShot capture and turnaround |
| `serialirq()` (~580-609) | USART + DMA | serial RX/TX frame transport |
| SBUS/CRSF/EXBUS/HoTT regions | timer/UART/DMA/watchdog | protocol transport timing |
| CLI ISR regions | UART or timer | CLI byte transport / optional pulse I/O |

For AM13 this area should be split by transport semantics rather than by
specific timer/UART register APIs.

### `src/telem.c`

Telemetry protocol formatting is mostly portable, but transport is not.

Direct hardware responsibilities include:

- UART baud/inversion/half-duplex/timeout configuration;
- RX/TX DMA setup;
- UART and DMA ISR flag handling;
- critical sections around shared telemetry buffers.

The portable boundary should preserve packet/protocol code and move only the
physical transport behind a small telemetry transport contract.

### `src/util.c`

Three hardware domains are mixed with otherwise portable utility code:

1. persistent configuration flash programming in `savecfg()`;
2. motor-output forcing and tone generation in `resetcom()`,
   `playmusic()`, and `playsound()`;
3. GPIO/clock-trim support in `initgpio()` and `hsictl()`.

`savecfg()` is especially important on AM13 because the application and the
persistent configuration area are both currently planned in Flash Bank0. The
AM13 implementation must follow TI flash execution restrictions and must not be
implemented as a direct translation of STM32 FLASH register writes.

### `mcu/STM32G071/config.c`

This file already acts as a partial platform layer and is useful as the
behavioral reference for the AM13 backend.

Existing semantic functions that are worth preserving or refining include:

```c
void init(void);
void compctl(int x);
void io_serial(void);
void io_analog(void);
void adctrig(void);
void initled(void);
void ledctl(int x);
void hsictl(int x);
```

The problem is that the platform boundary stops here; significant timer/UART/
DMA/flash behavior still lives in common sources.

## Dependency domains and AM13 mapping

| ESCape32 hardware need | Existing STM32/libopencm3 mechanism | AM13 candidate |
| --- | --- | --- |
| 3-phase complementary PWM | TIM1 advanced timer | MCPWM |
| synchronized commutation update | TIM1 COM/update events | MCPWM shadow/global-load/sync |
| dead time | TIM1 BDTR | MCPWM dead-band |
| hardware fault shutdown | break/output disable | MCPWM trip zone + CMPSS |
| BEMF comparator select/polarity | STM32 COMP + timer routing | CMPSS + routing/XBAR candidate |
| zero-cross timestamp | TIM2 input capture | eCAP or timer capture |
| throttle / pulse input | TIM3/TIM15 input capture | eCAP/TIMG candidate |
| DShot capture | timer + DMA | eCAP/TIMG + DMA candidate |
| BiDShot turnaround | timer mode switch + GPIO/DMA | capture + GPIO mux/direction + DMA |
| ADC sampling | ADC + DMA, software/external trigger | ADC + MCPWM SOC + DMA |
| telemetry / serial input | USART + DMA | UNICOMM UART + DMA |
| persistent config | STM32 FLASH register programming | TI flash DriverLib / FRI flow from RAM |
| watchdog/reset | IWDG/WWDG/reset flags | AM13 watchdog/SYSCTL |
| IRQ/vector control | libopencm3 NVIC/Cortex-M3 | CMSIS Cortex-M33 / TI arch |
| DroneCAN | not part of current rel17 path | MCAN + Libcanard |

Items marked as candidates still require routing/timing validation on real
AM13 hardware.

## Minimal ESCape32 hardware contract — provisional

Do not introduce a generic MCU HAL. The contract should expose only operations
needed by ESCape32.

A first cut is:

```c
/* CPU / critical sections */
void hw_irq_disable(void);
void hw_irq_enable(void);
void hw_request_deferred_work(void);

/* motor power stage */
void hw_motor_init(void);
void hw_motor_enable(void);
void hw_motor_disable(void);
void hw_motor_set_pwm_period(uint32_t ticks);
void hw_motor_set_phase_duty(uint16_t a, uint16_t b, uint16_t c);
void hw_motor_apply_commutation(uint8_t state);
void hw_motor_force_safe(void);

/* BEMF / commutation timing */
void hw_bemf_select(uint8_t phase, bool rising);
void hw_bemf_capture_arm(uint32_t timeout_ticks);
void hw_bemf_capture_disarm(void);
uint32_t hw_bemf_capture_value(void);

/* input capture */
void hw_input_init(void);
void hw_input_capture_arm(void);
void hw_input_capture_reset(void);

/* ADC */
void hw_adc_init(void);
void hw_adc_trigger(void);

/* serial transport */
void hw_input_serial_configure(...);
void hw_telem_serial_configure(...);

/* persistent configuration */
int hw_config_write(const void *data, size_t len);

/* reset / watchdog */
void hw_watchdog_kick(void);
void hw_system_reset(void);
```

This API is deliberately provisional. It should be reduced or reshaped while
the first vertical slice is implemented. In particular, hot-path calls must not
introduce avoidable abstraction overhead; static inline backend hooks or direct
backend calls are acceptable where deterministic latency matters.

## What should remain portable

The following logic should stay in common ESCape32 code wherever practical:

- six-step state machine and commutation policy;
- startup/ramp/duty policy;
- DShot bit/frame decode and validation;
- telemetry packet/protocol formatting;
- sensor scaling/filtering/protection policy;
- configuration semantics;
- CLI command parsing;
- motor/prop tuning parameters.

Only acquisition, actuation, timestamping, transport and non-volatile memory
operations should cross the hardware contract.

## Migration order

1. Finish Stage-A AM13 image/linker baseline.
2. Introduce the hardware-contract include boundary without changing existing
   STM32 behavior.
3. Move motor PWM/commutation register operations behind the contract.
4. Implement the AM13 MCPWM backend and validate waveforms on a scope.
5. Move BEMF selection/capture behind the contract and implement CMPSS +
   capture routing.
6. Move throttle/DShot/BiDShot capture behind the contract.
7. Move ADC trigger/DMA sampling.
8. Move UART/telemetry transport.
9. Move flash/config and watchdog/reset behavior.
10. Only after those boundaries are proven, compile the full ESCape32 rel17
    control path for AM13 and begin motor validation.

## Non-goals

- Do not port libopencm3 to AM13E.
- Do not create fake STM32 register definitions on AM13.
- Do not rewrite existing STM32 targets during the initial AM13 port.
- Do not introduce an RTOS merely to create an abstraction boundary.
- Do not combine the platform port with unrelated ESCape32 feature changes.

## Immediate next slice

The next code change should be small and reversible:

1. add an ESCape32 hardware-contract header;
2. provide an existing-STM32 backend that preserves the current behavior;
3. move only the motor PWM/commutation operations used by
   `nextstep()`/`laststep()` behind that boundary;
4. keep all existing targets building before implementing the AM13 MCPWM
   backend.

This gives the AM13 port a stable semantic boundary without forcing a
repository-wide HAL rewrite.
