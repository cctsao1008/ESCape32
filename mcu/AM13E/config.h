/* Compact native TI target selection; board_io.h owns physical routing.
 * Tokens resolve through DriverLib only in hardware consumers, so Core
 * and Boot do not acquire Application runtime dependencies. */
#pragma once
#ifndef AM13E
#error "AM13E Application configuration included without AM13E"
#endif
#include "hal/board_io.h"
#ifndef AM13E_IO_TIMER_INDEX
#define AM13E_IO_TIMER_INDEX 4
#endif
#ifndef AM13E_IO_DMA_CHANNEL
#define AM13E_IO_DMA_CHANNEL 2U
#endif
#ifndef AM13E_IO_CAPTURE_CHANNEL
#define AM13E_IO_CAPTURE_CHANNEL 0U
#endif
#if AM13E_IO_TIMER_INDEX!=4 || AM13E_IO_DMA_CHANNEL!=2 || AM13E_IO_CAPTURE_CHANNEL!=0
#error "PA26 requires TIMG4 CCP0 and DMA0 channel2; unsupported command resource combination"
#endif
#define IO_GPIO AM13E_COMMAND_GPIO
#define IO_PIN AM13E_COMMAND_MASK
#define IO_PINCM AM13E_COMMAND_PINCM
#define IOTIM TIMG4
#define IOTIM_IRQ TIMG4_0_INT_IRQn
#define IOTIM_CAPTURE_FUNCTION IOMUX_PA26_TIMG4_0_CCP0
#define IOTIM_CAPTURE_CHANNEL DL_TIMER_CC_0_INDEX
#define IOTIM_DMA DMA0
#define IOTIM_DMA_CHANNEL AM13E_IO_DMA_CHANNEL
#define IOTIM_DMA_IRQ DMA0_INT_IRQn
#define IOTIM_DMA_TRIGGER DL_DMA_TRIGGER_SOURCE_TIMG4_0_GEN_EVENT1
#define IOTIM_CAPTURE_SIZE 32U
#define IFTIM TIMG12
#define BEMF_CAPTURE ECAP1
#define MOTOR_PWM MCPWM4
#define MOTOR_PWM_IRQ MCPWM4_INT_IRQn
#define MOTOR_PWM_HANDLER MCPWM4_IRQHandler

/* UNICOMM instance selection is compile-time; pad/AF validity remains
 * checked against the selected board route by its native UART consumer. */
#define AM13E_UC_INST_(n) UC##n##_INST_PTR
#define AM13E_UC_INST(n) AM13E_UC_INST_(n)
#define AM13E_UC_IRQ_(n) UC##n##_INT_IRQn
#define AM13E_UC_IRQ(n) AM13E_UC_IRQ_(n)
#define AM13E_UC_HANDLER_(n) UC##n##_IRQHandler
#define AM13E_UC_HANDLER(n) AM13E_UC_HANDLER_(n)
#ifdef AM13E_BOARD_SERIAL_RX_UC_INDEX
#if AM13E_BOARD_SERIAL_RX_UC_INDEX<0 || AM13E_BOARD_SERIAL_RX_UC_INDEX>5
#error "Select AM13E UNICOMM0..UNICOMM5"
#endif
#define IO_UART AM13E_UC_INST(AM13E_BOARD_SERIAL_RX_UC_INDEX)
#define IO_UART_IRQ AM13E_UC_IRQ(AM13E_BOARD_SERIAL_RX_UC_INDEX)
#define IO_UART_HANDLER AM13E_UC_HANDLER(AM13E_BOARD_SERIAL_RX_UC_INDEX)
#endif
#ifdef AM13E_BOARD_TELEM_UC_INDEX
#if AM13E_BOARD_TELEM_UC_INDEX<0 || AM13E_BOARD_TELEM_UC_INDEX>5
#error "Telemetry UC selection must be UNICOMM0..5"
#endif
#define TELEM_UART AM13E_UC_INST(AM13E_BOARD_TELEM_UC_INDEX)
#define TELEM_UART_IRQ AM13E_UC_IRQ(AM13E_BOARD_TELEM_UC_INDEX)
#define TELEM_UART_HANDLER AM13E_UC_HANDLER(AM13E_BOARD_TELEM_UC_INDEX)
#endif
