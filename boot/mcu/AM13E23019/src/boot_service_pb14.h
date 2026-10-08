/*
 * AM13E23019 PB14 service transport.
 *
 * PB14/GPIO46 is the fixed physical service interface for the AM13E boot
 * architecture. The byte timing implementation is detailed design.
 */
#pragma once

#include <stdbool.h>

#include "boot_service_port.h"

bool boot_service_pb14_init(void);
const boot_service_transport_ops_t *boot_service_pb14_transport(void);
