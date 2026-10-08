/* Rel17 command symbols for E62 bring-up.
 * No persistence, audio, command transport, or motor integration is claimed.
 */
#include "am13e_prog_port.h"
#include <string.h>
#include <stdio.h>

/* cfg's immutable product identity is deliberately explicit. */
Cfg cfg = {.id = 0xE62, .revision = 17, .revpatch = 0, .name = "E62-AM13E",
    .throt_min = 1000, .throt_mid = 1500, .throt_max = 2000,
    .analog_min = 100, .analog_max = 3200,
    .freq_min = 24, .freq_max = 48, .duty_min = 1, .duty_max = 100,
    .timing = 16, .sine_power = 8, .duty_spup = 15, .duty_rate = 30,
    .telem_poles = 14, .volume = 25, .beacon = 50,
    .music = "dfa#" };
int throt, erpm, temp1, temp2, volt, curr, csum, beepval;
char analog, telphid, rearm;

void checkcfg(void) { esc_checkcfg(&cfg); }
/* Explicitly unsupported until real nonvolatile configuration and
 * audio backend plus safety/authorization are implemented. */
int savecfg(void) { return 0; }
int resetcfg(void) { return 0; }
int playmusic(const char *music, int volume)
{
    (void)music;
    (void)volume;
    return 0;
}
char *am13e_prog_itoa(int value, char *out, int base)
{
    if (base != 10) { out[0] = 0; return out; }
    (void)snprintf(out, 12, "%d", value);
    return out;
}
char *am13e_prog_stpcpy(char *dst, const char *src)
{
    while ((*dst = *src) != 0) { ++dst; ++src; }
    return dst;
}
char *am13e_prog_strsep(char **str, const char *sep)
{
    char *start = *str;
    if (!start) return NULL;
    char *end = start;
    while (*end) {
        if (strchr(sep, *end)) {
            *end = 0;
            *str = end + 1;
            return start;
        }
        ++end;
    }
    *str = NULL;
    return start;
}
size_t am13e_prog_strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = 0;
    }
    return len;
}
