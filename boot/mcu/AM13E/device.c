/*
** AM13E boot device identification and reset-cause reporting.
** Preserve the ESCape32 CMD_INFO device-ID byte layout.
*/
#include "common.h"
#include <dl_sysctl.h>
#include <hw_factoryregion.h>

uint32_t boot_am13e_device_id(void) {
    return FACTORYREGION->DEVICEID;
}

bool boot_am13e_take_reboot_ack(void) {
    /* The original bootloader sends an ACK following a software reset.
     * A power-on, brown-out, external reset or fault must not be mistaken
     * for a software-requested reboot.
     */
    switch (DL_SYSCTL_getResetCause()) {
    case DL_SYSCTL_RESET_CAUSE_BOOTSW:
    case DL_SYSCTL_RESET_CAUSE_SYSSW:
    case DL_SYSCTL_RESET_CAUSE_CPUSW:
        return true;
    default:
        return false;
    }
}
