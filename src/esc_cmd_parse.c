/* ESCape32 rel17 command parser primitives; independent of MCU services. */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include "esc_cmd_parse.h"

int esc_cmd_getidx(const char *str, const char *const vec[])
{
    int idx = 0;
    const char *const *pos = vec;
    for (const char *val; (val = *pos++) && strcasecmp(val, str); ++idx);
    return idx;
}

int esc_cmd_getval(const char *str, int *val)
{
    char *end;
    long res = strtol(str, &end, 10);
    if (*end || res < INT_MIN || res > INT_MAX) return 0;
    *val = (int)res;
    return 1;
}

int esc_cmd_getint(const char *buf, int len)
{
    uint32_t res = 0;
    while (len--) res = (res << 8) | (uint8_t)*buf++;
    return (int)res;
}
