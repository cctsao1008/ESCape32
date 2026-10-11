#include "telem_mode_plan.h"
#include <stddef.h>
int am13e_telem_mode_plan(int mode, AM13E_TelemModePlan *out)
{
    static const AM13E_TelemModePlan modes[7] = {
        {115200U, 10U, 0U, 0U, 0U, 1U}, /* KISS: legacy HDSEL */
        {115200U, 10U, 0U, 0U, 0U, 1U}, /* KISS auto */
        {115200U, 10U, 1U, 0U, 1U, 1U}, /* iBUS: RX->TX */
        { 57600U, 26U, 1U, 1U, 1U, 1U}, /* S.Port inverted; SW gap required */
        {416666U, 10U, 0U, 0U, 0U, 1U}, /* CRSF */
        { 38400U, 10U, 1U, 0U, 1U, 1U}, /* MSB: RX->TX */
        { 19200U, 10U, 1U, 0U, 1U, 1U}, /* HoTT deferred-response */
    };
    if (out == NULL || mode < 0 || mode >= 7) return 0;
    *out = modes[mode];
    return 1;
}

int am13e_telem_rx_timeout_register(const AM13E_TelemModePlan *plan,
                                    uint8_t *field_value)
{
    /* TI SDK DL_UART_setRXInterruptTimeout accepts only [0,15].
     * Rel17 RTOR units/packet boundary need separate silicon proof.
     * This helper only checks field REPRESENTABILITY, not timing parity.
     */
    if (plan == NULL || field_value == NULL ||
        !plan->rx_protocol || plan->rx_timeout_bits == 0U ||
        plan->rx_timeout_bits > 15U) return 0;
    *field_value = (uint8_t)plan->rx_timeout_bits;
    return 1;
}
