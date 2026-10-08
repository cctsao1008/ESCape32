/*
 * AM13E23019 PB14/GPIO46 Boot service transport.
 *
 * Software implementation: polling 38400-baud 8N1 service link with a
 * 500-ms receive timeout, using the deterministic Boot SYSOSC clock basis.
 * Electrical/timing validation remains a Phase-4 hardware activity.
 */
#pragma once

#include <stdbool.h>

#include "boot_service_port.h"

bool boot_service_pb14_init(void);
const boot_service_transport_ops_t *boot_service_pb14_transport(void);
