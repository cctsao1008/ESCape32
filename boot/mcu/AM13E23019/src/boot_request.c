/*
 * AM13E23019 application-to-boot request contract.
 */

#include "boot_request.h"

boot_request_reason_t boot_request_detect(void)
{
    /*
     * PSEUDOCODE / detailed design:
     *
     * retained = read reset-retained mailbox/register
     * if retained.magic == BOOT_REQUEST_MAGIC &&
     *    retained.complement == ~BOOT_REQUEST_MAGIC:
     *     return BOOT_REQUEST_SERVICE
     *
     * return BOOT_REQUEST_NONE
     *
     * Application-side producer:
     *   1. stop motor and force power stage safe
     *   2. quiesce service/runtime interface
     *   3. write boot-request record
     *   4. DSB/ISB as required
     *   5. system reset
     */
    return BOOT_REQUEST_NONE;
}

void boot_request_consume(boot_request_reason_t reason)
{
    (void)reason;

    /*
     * PSEUDOCODE / detailed design:
     *
     * if reason != BOOT_REQUEST_NONE:
     *     clear retained request before accepting programming commands
     *     verify clear
     *
     * The request must be one-shot so a later normal reset does not
     * permanently trap the product in Boot.
     */
}
