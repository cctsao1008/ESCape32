/* ESCape32 Rel17 v1.4 AM13E Application entry bridge.
 * TI SDK startup_gcc_arm.c declares extern int main(void). Keep one
 * startup/exception vector provider in the TI startup object and
 * preserve Rel17's original void control-loop body in src/main.c.
 * This file does not initialize clocks, peripherals or control policy.
 */
void am13e_rel17_app_main(void);

int main(void)
{
    am13e_rel17_app_main();
    /* Rel17's main loop should never return. Do not restart firmware
     * or jump to a different image on an unexpected return.
     */
    for (;;) {
    }
}
