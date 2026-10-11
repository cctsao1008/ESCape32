/* Safe UC2 UART peripheral preflight only.
 *
 * PA22=UC2_TX, PA23=UC2_RX. The ESCape32 Rel17 wire is single-wire
 * and requires a qualified board-level transmit/receive interface.
 * This module deliberately DOES NOT connect either pin to UART or
 * release any UART interrupts. It is a real UC2 register/clock test,
 * NOT an implementation of am13e_telem_hw_* callbacks.
 */
#pragma once
#if !defined(AM13E)
#error "AM13E UC2 UART only"
#endif
#include <stdint.h>
/* Success means UC2 powered, configured and disabled-direction; it
 * does not mean an external UART protocol can work.
 */
int am13e_uc2_uart_preflight(uint32_t baud, uint32_t timeout_field);
int am13e_uc2_uart_is_prepared(void);
