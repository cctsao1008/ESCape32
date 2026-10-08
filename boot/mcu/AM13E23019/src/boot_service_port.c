/*
 * AM13E23019 boot service adapter.
 */

#include "boot_service_port.h"

#include <stddef.h>

#include "boot_image.h"
#include "boot_port.h"
#include "crc32.h"
#include "service_io.h"

static const boot_service_transport_ops_t *active_transport;

static int service_recv_buffer(char *buffer, int length)
{
    return active_transport->recv_buffer(buffer, length);
}

static void service_send_buffer(const char *buffer, int length)
{
    active_transport->send_buffer(buffer, length);
}

uint32_t boot_service_port_crc32(const char *buffer, int length)
{
    if ((buffer == NULL) || (length < 0)) {
        return 0U;
    }

    return boot_crc32(buffer, (uint32_t)length);
}

static const boot_service_io_ops_t service_io_ops = {
    .recv_buffer = service_recv_buffer,
    .send_buffer = service_send_buffer,
    .crc32 = boot_service_port_crc32,
};

static int service_recv_value(void)
{
    return boot_service_recv_value(&service_io_ops);
}

static void service_send_value(int value)
{
    boot_service_send_value(&service_io_ops, value);
}

static int service_recv_data(char *buffer)
{
    return boot_service_recv_data(&service_io_ops, buffer);
}

static void service_send_data(const char *buffer, int length)
{
    boot_service_send_data(&service_io_ops, buffer, length);
}

static uint32_t service_device_id(void)
{
    return active_transport->device_id();
}

const boot_protocol_ops_t *boot_service_port_bind(
    const boot_service_transport_ops_t *transport)
{
    static boot_protocol_ops_t protocol_ops = {
        .recv_value = service_recv_value,
        .send_value = service_send_value,
        .recv_data = service_recv_data,
        .send_data = service_send_data,
        .device_id = service_device_id,
        .map_read = boot_port_map_read,
        .write_block = boot_port_write_block,
        .handle_update = NULL,
        .handle_setwrp = NULL,
        .app_valid = boot_image_launchable,
        .jump_app = boot_port_jump_to_app,
    };

    if ((transport == NULL) ||
        (transport->recv_buffer == NULL) ||
        (transport->send_buffer == NULL) ||
        (transport->device_id == NULL)) {
        active_transport = NULL;
        return NULL;
    }

    active_transport = transport;
    protocol_ops.io_id = transport->io_id;

    return &protocol_ops;
}
