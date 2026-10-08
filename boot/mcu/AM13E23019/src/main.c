/*
 * AM13E23019 common bootloader entry.
 *
 * Common command semantics live under boot/src. This target-specific entry
 * keeps only the startup decision until the AM13E service-I/O backend is wired
 * to the common protocol engine.
 */

#include "boot_port.h"
#include "soc.h"

int main(void)
{
    if (boot_port_app_valid()) {
        boot_port_jump_to_app();
    }

    /*
     * Invalid/incomplete application: remain in Boot until the service-I/O
     * backend is connected to boot_protocol_run().
     */
    for (;;) {
        __WFI();
    }
}
