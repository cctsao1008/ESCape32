/* Bounded Rel17 CRSF command deferral from UART IRQ to PendSV.
 * One outstanding command; no allocation, overwrite or IRQ Flash.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define AM13E_CRSF_PENDING_MAX 64U
typedef struct {
    uint8_t frame[AM13E_CRSF_PENDING_MAX];
    unsigned length;
    unsigned busy;
    unsigned drops;
} AM13E_CrsfDeferredCommand;
bool am13e_crsf_deferred_begin(AM13E_CrsfDeferredCommand *q,
                               const uint8_t *p,unsigned n);
bool am13e_crsf_deferred_take(AM13E_CrsfDeferredCommand *q,
                              uint8_t *out,unsigned capacity,
                              unsigned *length);
void am13e_crsf_deferred_finish(AM13E_CrsfDeferredCommand *q);
