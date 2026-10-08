#include <assert.h>
#include <limits.h>
#include "esc_cmd_parse.h"

int main(void)
{
    static const char *const names[] = {"help", "show", "set", 0};
    int v = 0;
    assert(esc_cmd_getidx("HELP", names) == 0);
    assert(esc_cmd_getidx("Set", names) == 2);
    assert(esc_cmd_getidx("unknown", names) == 3);
    assert(esc_cmd_getval("123", &v) && v == 123);
    assert(esc_cmd_getval("-45", &v) && v == -45);
    assert(!esc_cmd_getval("12x", &v));
    assert(!esc_cmd_getval("2147483648", &v));
    assert(!esc_cmd_getval("-2147483649", &v));
    const char bytes[] = {(char)0x80, (char)0xff, (char)0x12, (char)0x34};
    assert(esc_cmd_getint(bytes, 1) == 0x80);
    assert(esc_cmd_getint(bytes, 2) == 0x80ff);
    assert(esc_cmd_getint(bytes + 2, 2) == 0x1234);
    assert(esc_cmd_getint(bytes, 0) == 0);
    (void)INT_MAX;
    return 0;
}
