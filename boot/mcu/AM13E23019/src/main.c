/*
 * AM13E23019 common bootloader entry.
 *
 * Common command semantics live under boot/src. The physical service transport
 * is selected independently through boot_service_transport_get().
 */

#include "boot_port.h"
#include "boot_service_port.h"
#include "protocol.h"
#include "soc.h"

int main(void)
{
    const boot_service_transport_ops_t *transport =
        boot_service_transport_get();

    if (transport != NULL) {
        const boot_protocol_ops_t *protocol =
            boot_service_port_bind(transport);

        if (protocol != NULL) {
            /*
             * The transport recv path owns its timeout. A receive timeout
             * preserves upstream ESCape32 behavior: boot_protocol_run() tries
             * the installed application through the common app_valid/jump
             * callbacks.
             */
            boot_protocol_run(protocol);
        }
    }

    if (boot_port_app_valid()) {
        boot_port_jump_to_app();
    }

    /*
     * No usable transport and no valid application: remain in Boot/recovery.
     */
    for (;;) {
        __WFI();
    }
}
