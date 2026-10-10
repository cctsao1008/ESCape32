/*
 * Generic AM13E MCU backend for asynchronous MCPWM fault trips.
 * A selected board supplies its GPIO index, route, polarity and OST.
 * This module NEVER configures physical gate outputs or clears a trip.
 */
#pragma once
#ifndef AM13E
#error "AM13E MCU backend required"
#endif
#include <stdbool.h>
#include <stdint.h>
#include <dl_xbar.h>
#include <dl_mcpwm.h>

typedef struct {
    MCPWM_Regs *mcpwm;
    uint8_t gpio_index;
    DL_XBAR_InputNum input_xbar;
    DL_XBAR_TripNum pwm_trip;
    DL_XBAR_PWMXBARSource pwm_source;
    uint32_t ost_signal;
    uint32_t ost_flag;
    bool active_low;
} AM13E_FaultTripRoute;

/* Software route consistency only, NOT electrical qualification. */
int am13e_mcu_fault_trip_route_valid(const AM13E_FaultTripRoute *route);
/* PRIMASK must be set; invalid input halts with interrupts disabled. */
void am13e_mcu_fault_trip_install(const AM13E_FaultTripRoute *route);
/* Returns actual XBAR / MCPWM readback, never an assumed healthy value. */
int am13e_mcu_fault_trip_ready(const AM13E_FaultTripRoute *route);
