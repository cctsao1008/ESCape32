/* The same ESCape32 rel17 parameter IDs and names used in prog.c. */
#include <stddef.h>
#include "esc_param_metadata.h"
#include "esc_cmd_parse.h"

#define XX(idx, type, key, def, opt, dec, min, max, step, exp1, exp2) #key,
static const char *const names[] = { CFG_MAP(XX) NULL };
#undef XX

_Static_assert(sizeof(names) / sizeof(names[0]) == PARAM_CNT + 1,
               "ESCape32 parameter count mismatch");

unsigned esc_param_count(void) { return PARAM_CNT; }
const char *esc_param_name(unsigned id)
{
    return id < PARAM_CNT ? names[id] : NULL;
}
int esc_param_find(const char *name)
{
    return name ? esc_cmd_getidx(name, names) : -1;
}
