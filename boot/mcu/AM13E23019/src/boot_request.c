/*
 * AM13E23019 application-to-Boot request contract.
 */

#include "boot_request.h"

#include <stdint.h>

#include "dl_sysctl.h"
#include "soc.h"

#define BOOT_REQUEST_MAGIC          0xB6U
#define BOOT_REQUEST_MAGIC_INV      ((uint8_t)~BOOT_REQUEST_MAGIC)
#define BOOT_REQUEST_SERVICE_VALUE  ((uint8_t)BOOT_REQUEST_SERVICE)
#define BOOT_REQUEST_SERVICE_INV    ((uint8_t)~BOOT_REQUEST_SERVICE_VALUE)

static void boot_request_clear_storage(void)
{
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_0, 0U);
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_1, 0U);
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_2, 0U);
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_3, 0U);
}

boot_request_reason_t boot_request_detect(void)
{
    uint8_t magic = DL_SYSCTL_getShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_0);
    uint8_t magic_inv = DL_SYSCTL_getShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_1);
    uint8_t reason = DL_SYSCTL_getShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_2);
    uint8_t reason_inv = DL_SYSCTL_getShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_3);

    if ((magic != BOOT_REQUEST_MAGIC) ||
        (magic_inv != BOOT_REQUEST_MAGIC_INV) ||
        (((uint8_t)(reason ^ reason_inv)) != 0xffU)) {
        return BOOT_REQUEST_NONE;
    }

    if (reason == BOOT_REQUEST_SERVICE_VALUE) {
        return BOOT_REQUEST_SERVICE;
    }

    return BOOT_REQUEST_NONE;
}

void boot_request_consume(boot_request_reason_t reason)
{
    if (reason != BOOT_REQUEST_NONE) {
        /*
         * One-shot semantics: clear before entering the programming path so a
         * later reset cannot permanently trap the product in Boot.
         */
        boot_request_clear_storage();
        __DSB();
        __ISB();
    }
}

__attribute__((noreturn))
void boot_request_service_reset(void)
{
    /*
     * This API intentionally does not perform motor shutdown. FW1/FW2 owns
     * that product-safety sequence and calls this only after the power stage
     * and runtime command path are quiescent.
     */
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_0, BOOT_REQUEST_MAGIC);
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_1, BOOT_REQUEST_MAGIC_INV);
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_2, BOOT_REQUEST_SERVICE_VALUE);
    DL_SYSCTL_setShutdownStorageByte(
        DL_SYSCTL_SHUTDOWN_STORAGE_BYTE_3, BOOT_REQUEST_SERVICE_INV);

    __DSB();
    __ISB();

    DL_SYSCTL_resetDevice(DL_SYSCTL_RESET_SYS);

    for (;;) {
        __WFI();
    }
}
