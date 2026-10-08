/*
 * AM13E23019 Stage-A bring-up image.
 *
 * Purpose:
 *   - prove the TI SDK/GCC/SysConfig application can link at APP_BASE 0x6000;
 *   - make no ESCape32 motor-control behavior changes yet.
 *
 * The known-good TI empty-example SysConfig output is reused here so startup,
 * clock and flash initialization remain comparable with the SDK smoke target.
 */

#include "ti_sdk_dl_config.h"

int main(void)
{
    SYSCFG_DL_init();

    for (;;) {
        /* Build-only baseline. Hardware bring-up follows in the next stage. */
    }
}
