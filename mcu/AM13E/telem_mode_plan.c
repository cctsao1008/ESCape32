#include "telem_mode_plan.h"
#include <stddef.h>
int am13e_telem_mode_plan(int mode, AM13E_TelemModePlan *out)
{
    static const AM13E_TelemModePlan modes[7] = {
        {115200U, 10U, 0U, 0U, 0U}, /* KISS */
        {115200U, 10U, 0U, 0U, 0U}, /* KISS auto */
        {115200U, 10U, 1U, 0U, 0U}, /* iBUS */
        { 57600U, 26U, 1U, 1U, 1U}, /* S.Port: inverted single-wire */
        {416666U, 10U, 0U, 0U, 0U}, /* CRSF */
        { 38400U, 10U, 1U, 0U, 0U}, /* MSB */
        { 19200U, 10U, 1U, 0U, 1U}, /* HoTT deferred-response */
    };
    if (out == NULL || mode < 0 || mode >= 7) return 0;
    *out = modes[mode];
    return 1;
}
