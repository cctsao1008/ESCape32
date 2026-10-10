/* ESCape32 Rel17 v1.4 AM13E Boot entry bridge.
 * The single TI SDK Reset_Handler declares extern int main(void);
 * Boot's original protocol dispatcher retains a void-return contract
 * in boot/src/main.c and its legacy MCU entry is not modified.
 * No additional vector table, framework, Boot ABI or parser is added.
 */
void am13e_rel17_boot_main(void);

int main(void)
{
    am13e_rel17_boot_main();
    /* Unexpected return from the original Boot command loop is not a
     * successful APP handoff. Remain in Boot rather than returning to
     * TI Startup or introducing a second reset/launch path.
     */
    for (;;) {
    }
}
