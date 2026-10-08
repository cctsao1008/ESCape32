/*
 * AM13E23019 common bootloader entry.
 *
 * Architecture-first orchestration:
 *
 *   reset
 *     -> detect one-shot application-to-boot request
 *     -> initialize fixed PB14 service transport
 *     -> run common ESCape32 service/programming protocol when available
 *     -> validate installed application
 *     -> jump to APP or remain in recovery
 *
 * Detailed physical timing, image-record format, and retained boot-request
 * encoding may still be pseudocode, but the module boundaries are fixed here.
 */

#include "boot_image.h"
#include "boot_port.h"
#include "boot_request.h"
#include "boot_service_pb14.h"
#include "boot_service_port.h"
#include "protocol.h"
#include "soc.h"

int main(void)
{
    boot_request_reason_t request = boot_request_detect();

    if (request != BOOT_REQUEST_NONE) {
        /*
         * Consume the one-shot request before accepting programming commands.
         * A future implementation may use the reason to extend/force the PB14
         * service window rather than taking the normal timeout path.
         */
        boot_request_consume(request);
    }

    /*
     * PB14/GPIO46 is the fixed AM13 service physical interface. The current
     * PB14 module is an architecture scaffold and returns false until its
     * timing/IOMUX implementation is ready.
     */
    if (boot_service_pb14_init()) {
        const boot_protocol_ops_t *protocol =
            boot_service_port_bind(boot_service_pb14_transport());

        if (protocol != NULL) {
            /*
             * The physical receive path owns the bounded timeout. Upstream
             * ESCape32 semantics are preserved: timeout/invalid command gives
             * the protocol engine an opportunity to launch a valid APP.
             */
            boot_protocol_run(protocol);
        }
    }

    if (boot_image_launchable()) {
        boot_port_jump_to_app();
    }

    /*
     * No launchable application: stay in Boot/recovery. Once PB14 transport is
     * implemented, an invalid image remains serviceable through the common
     * programming protocol instead of falling through to APP.
     */
    for (;;) {
        __WFI();
    }
}
