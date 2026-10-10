/*
 * ESCape32 Rel17 AM13E Application input/DSHOT platform contract.
 *
 * PB14 GPIO46/INPUTXBAR1/ECAP0 captures DShot/PWM RX; TIMG4/DMA0
 * drives the NRZI telemetry reply on GPIO46, then restores ECAP0 RX.
 * Timing and external-line electrical validation remain outstanding.
 *
 * initio() now owns PB14 input and ECAP0 edge timestamps. The
 * bidirectional TX waveform and direction changes have implementations;
 * the WWDT0 valid-frame feed backend is implemented; service/CLI transport
 * and silicon watchdog/timing validation are pending.
 * No STM32 TIMx register aliases are used.
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
 * am13e_app_io_watchdog_feed() starts/restarts real WWDT0 only when
 * original Rel17 callbacks accept a physical PWM/DSHOT frame. Do not
 * refresh from SysTick or pass CRC-invalid frames.
 * am13e_app_io_servo_pulse() is passed a *validated* 800..2200us width;
 * caller must qualify timing/faults and feed watchdog as required.
 * Other physical PWM/Oneshot/serial input event surfaces are pending.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E application input contract only"
#endif

#include <stdint.h>
#include "bidir_codec.h"

/* Original Rel17 telemetry selection + GCR/NRZI levels. Physical
 * TX scheduler must call after a CRC-valid inverted DShot command.
 */
void am13e_app_io_bidir_telemetry_levels(
    uint8_t levels[AM13E_BIDIR_DMA_LEVELS]);

int am13e_app_io_dshot_packet(uint16_t frame, int bidirectional_invert);
void am13e_app_io_servo_pulse(unsigned int pulse_us);
/* Valid original 32-byte iBUS receiver frame; selected UART transport
 * owns clock/pinmux, parity, frame acquisition and resynchronization.
 */
int am13e_app_io_ibus_frame(const uint8_t *frame,unsigned length);
int am13e_app_io_cli_line(char *line);

/* Must be implemented by the AM13E board backend. */
void am13e_app_io_watchdog_feed(void);

/* GPIO1 vector sharing contract: called ONLY for enabled non-PB15
 * GPIO1 interrupts. The physical PB14 input backend must identify and
 * acknowledge its own source(s). Do not provide a link-only no-op.
 */
void am13e_app_io_on_gpio1_interrupt(uint32_t pending);

/* Lowest-priority PendSV services DShot Save after PB14 TX completes.
 * Never execute erase/program synchronously inside ECAP0 IRQ.
 */
void am13e_app_io_service_pending_save(void);
