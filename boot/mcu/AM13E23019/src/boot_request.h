/*
 * AM13E23019 application-to-Boot request contract.
 *
 * The request is carried in SYSCTL SHUTDNSTORE bytes, which are retained
 * across SYSRST and cleared by lower-level reset events such as POR.
 */
#pragma once

#include <stdbool.h>

typedef enum {
    BOOT_REQUEST_NONE = 0,
    BOOT_REQUEST_SERVICE = 1,
} boot_request_reason_t;

boot_request_reason_t boot_request_detect(void);
void boot_request_consume(boot_request_reason_t reason);

/*
 * Application-side producer for a service/update reboot.
 * The caller must place the motor/power stage in a safe state before calling.
 */
__attribute__((noreturn))
void boot_request_service_reset(void);
