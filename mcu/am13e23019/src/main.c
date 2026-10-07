/*
 * AM13E23019 Stage-A bring-up image.
 *
 * Purpose:
 *   - prove the TI SDK/GCC/SysConfig application can link at APP_BASE 0x6000;
 *   - make no ESCape32 motor-control behavior changes yet.
 *
 * Peripheral initialization is intentionally deferred to the next bring-up
 * stage. SysConfig is still part of the target because TI's generated platform
 * configuration and startup libraries remain the baseline.
 */

int main(void)
{
    for (;;) {
        /* Build-only baseline. Hardware bring-up follows in the next stage. */
    }
}
