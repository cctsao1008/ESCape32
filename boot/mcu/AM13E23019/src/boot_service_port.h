/*
 * AM13E23019 boot service adapter.
 *
 * This layer connects a target byte transport to the common ESCape32 framing
 * and command engine. The physical transport itself is intentionally not
 * selected here.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"

typedef struct {
    uint8_t io_id;
    int (*recv_buffer)(char *buffer, int length);
    void (*send_buffer)(const char *buffer, int length);
    uint32_t (*device_id)(void);
} boot_service_transport_ops_t;

/*
 * Bind a physical byte transport and return the common protocol adapter.
 * Returns NULL when the transport contract is incomplete.
 */
const boot_protocol_ops_t *boot_service_port_bind(
    const boot_service_transport_ops_t *transport);

/* Software CRC-32 used by the AM13 boot service framing. */
uint32_t boot_service_port_crc32(const char *buffer, int length);
