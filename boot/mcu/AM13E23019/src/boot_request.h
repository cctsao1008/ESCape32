/*
 * AM13E23019 application-to-boot request contract.
 *
 * The application-side producer and boot-side retention mechanism are detailed
 * design. This file keeps the architecture visible before the retention
 * register/mailbox choice is frozen.
 */
#pragma once

#include <stdbool.h>

typedef enum {
    BOOT_REQUEST_NONE = 0,
    BOOT_REQUEST_SERVICE,
} boot_request_reason_t;

boot_request_reason_t boot_request_detect(void);
void boot_request_consume(boot_request_reason_t reason);
