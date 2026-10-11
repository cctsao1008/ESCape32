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
 * Servo callback receives coherent captured width/period in original
 * timer counts, including telemetry/invalid widths after mode selection.
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
void am13e_app_io_servo_pulse(unsigned int pulse_ticks,unsigned int period_ticks);
/* Explicit observable gap: configured calibration without a native trim
 * provider retains previous uncalibrated reception, never claims trim.
 */
int am13e_app_io_servo_calibration_blocked(void);
#ifdef AM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED
#if AM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED != 1
#error "Servo clock trim requires an explicitly selected real board provider"
#endif
void am13e_app_servo_clock_trim(int direction);
#endif
/* Valid original 32-byte iBUS receiver frame; selected UART transport
 * owns clock/pinmux, parity, frame acquisition and resynchronization.
 */
int am13e_app_io_ibus_frame(const uint8_t *frame,unsigned length);
/* Complete 25-byte SBUS frame from board UART RX. Returns 1 if the
 * source-defined header/length checks pass in input_mode=4.
 * SBUS2 telemetry request/slot scheduling is a separate MCU gap.
 */
int am13e_app_io_sbus_frame(const uint8_t *frame,unsigned length);
/* Original six SBUS2 reply payloads with explicit delay per slot.
 * Actual RX mute/physical slot TX requires qualified hardware timer.
 */
int am13e_app_io_sbus2_next_slot(uint8_t out[3],unsigned *delay_us);
/* Original input_mode=2 binary serial control (not text CLI).
 * Return 1 only if wire CRC accepted; 0/4/12 response bytes.
 */
int am13e_app_io_serial_frame(const uint8_t *frame,unsigned length,
                              uint8_t *reply,unsigned reply_capacity,
                              unsigned *reply_length);
/* Original CRSF input_mode=5. Valid CRC refreshes WWDT; type 0x16
 * updates original throttle/brake. Command response TX still missing.
 */
int am13e_app_io_crsf_frame(const uint8_t *frame,unsigned length);
/* EXBUS input_mode=6: original CRC16ccitt and getchan2 semantics.
 * Valid telemetry request is recognized but never falsely ACKed/TX.
 */
int am13e_app_io_exbus_frame(const uint8_t *frame,unsigned length);
/* Construct source-defined EXBUS response from original Rel17 sensor
 * values. Caller must supply board-qualified stable device identity
 * and own a validated half-duplex TX path. Not used by Reference.
 */
unsigned am13e_app_io_exbus_telemetry(uint8_t request_id,
                                      uint32_t device_id,uint8_t *out,
                                      unsigned capacity);
/* HoTT/SUMD original mode=7 v1/v3 channel maps, physical timing
 * still requires board-qualified UNICOMM RX.
 */
int am13e_app_io_hott_frame(const uint8_t *frame,unsigned length);
/* Invoke the real Rel17 execcrsfcmd()/crc8dvbs2() only for a validated
 * CRSF input_mode=5 command, telmode=4; return completed wire bytes.
 */
unsigned am13e_app_io_crsf_reply(const uint8_t *frame,unsigned length,
                                 uint8_t *reply,unsigned capacity);
/* Read original Rel17 cfg.input_mode from src/io.c, avoiding
 * duplicate shared/MCU platform declarations in native IRQ modules.
 */
unsigned am13e_app_io_input_mode(void);
int am13e_app_io_service_idle(void);
int am13e_app_io_cli_line(char *line);

/* Must be implemented by the AM13E board backend. */
void am13e_app_io_watchdog_feed(void);
/* Rel17 starts input-loss supervision at receiver selection, before CRC. */
void am13e_app_io_watchdog_arm(void);

/* GPIO1 vector sharing contract: called ONLY for enabled non-PB15
 * GPIO1 interrupts. The physical PB14 input backend must identify and
 * acknowledge its own source(s). Do not provide a link-only no-op.
 */
void am13e_app_io_on_gpio1_interrupt(uint32_t pending);

/* Lowest-priority PendSV services DShot Save/Reset after PB14 TX completes.
 * Never execute erase/program synchronously inside ECAP0 IRQ.
 */
void am13e_app_io_service_pending_save(void);
