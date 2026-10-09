/*
 * ESCape32 Rel17 AM13E Application input/DSHOT platform contract.
 *
 * Stage E1-B: platform functions are declarations only; no peripheral
 * implementation is supplied, and Object Compile is NOT Link PASS.
 *
 * The physical input backend MUST implement initio() declared in
 * src/common.h. It must configure board-confirmed timer/capture/DMA,
 * input voltage/timing, watchdog, and, where applicable, serial/UART
 * and bidirectional DSHOT turnaround. No STM32 TIMx aliases permitted.
 *
 * Upon receipt of a complete 16-bit DSHOT frame (including 4-bit CRC),
 * call am13e_app_io_dshot_packet(). A return of 1 indicates a valid
 * frame, 0 indicates CRC rejection and requires capture resync. Parameter bidirectional_invert is
 * true only when the physical DSHOT RX path uses inverted CRC semantics.
 * This callback validates the CRC and executes the common Rel17 DSHOT
 * throttle/command logic. The caller MUST verify frame length, bit
 * timing, and physical sync separately BEFORE passing the frame.
 *
 * For a line already validated and terminated by the physical CLI
 * transport, call am13e_app_io_cli_line(); caller owns the buffer and
 * must ensure the line is NUL terminated and properly bounded.
 *
 * am13e_app_io_watchdog_feed() is a real hardware watchdog callback;
 * absent a real backend, final ELF must not link. No dummy feeding.
 * am13e_app_io_servo_pulse() is passed a *validated* 800..2200us width;
 * caller must qualify timing/faults and feed watchdog as required.
 * Other physical PWM/Oneshot/serial input event surfaces are pending.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E application input contract only"
#endif

#include <stdint.h>

int am13e_app_io_dshot_packet(uint16_t frame, int bidirectional_invert);
void am13e_app_io_servo_pulse(unsigned int pulse_us);
int am13e_app_io_cli_line(char *line);

/* Must be implemented by the AM13E board backend. */
void am13e_app_io_watchdog_feed(void);

/* GPIO1 vector sharing contract: called ONLY for enabled non-PB15
 * GPIO1 interrupts. The physical PB14 input backend must identify and
 * acknowledge its own source(s). Do not provide a link-only no-op.
 */
void am13e_app_io_on_gpio1_interrupt(uint32_t pending);
