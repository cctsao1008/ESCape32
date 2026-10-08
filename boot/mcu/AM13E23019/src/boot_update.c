/*
 * E62 AM13E application update transaction.
 */

#include "boot_update.h"

#include <stddef.h>

#include "boot_port.h"
#include "protocol.h"

static bool update_active;
static uint32_t expected_offset;

void boot_update_reset_session(void)
{
    update_active = false;
    expected_offset = 0U;
}

bool boot_update_session_active(void)
{
    return update_active;
}

uint32_t boot_update_next_offset(void)
{
    return expected_offset;
}

bool boot_update_write_block(
    uint32_t offset, const char *data, uint32_t length)
{
    if ((data == NULL) ||
        (length == 0U) ||
        (length > BOOT_PROTOCOL_BLOCK_SIZE) ||
        ((length & 0x3U) != 0U)) {
        return false;
    }

    /*
     * A new block-0 WRITE always starts/restarts the transaction. Erasing and
     * programming block 0 destroys the previous vector/header sector first,
     * so an interrupted update cannot leave the old image falsely launchable.
     */
    if (offset == 0U) {
        update_active = true;
        expected_offset = 0U;
    }

    if (!update_active || (offset != expected_offset)) {
        return false;
    }

    if (!boot_port_write_block(offset, data, length)) {
        return false;
    }

    expected_offset += length;
    return true;
}
