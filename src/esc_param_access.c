/* Rel17 CFG_MAP-backed numeric parameter access, separate from command execution. */
#include <stddef.h>
#include "esc_param_access.h"
#include "esc_param_map.h"
#include "esc_config.h"

#define ESC_GET_val(key) do { *value = cfg->key; return 1; } while (0)
#define ESC_GET_str(key) return 0
int esc_param_get_numeric(const Cfg *cfg, unsigned id, int *value)
{
    if (!cfg || !value) return 0;
    switch (id) {
#define XX(idx, type, key, def, opt, dec, lo, hi, step, exp1, exp2) \
        case idx: ESC_GET_##type(key);
        CFG_MAP(XX)
#undef XX
        default: return 0;
    }
}
#undef ESC_GET_val
#undef ESC_GET_str

/* Check the declared parameter range before narrowing to char / uint16_t. */
#define ESC_SET_val(key, lo, hi) do { \
    if (value < (lo) || value > (hi)) return 0; \
    cfg->key = value; \
    esc_checkcfg(cfg); \
    return 1; \
} while (0)
#define ESC_SET_str(key, lo, hi) return 0
int esc_param_set_numeric(Cfg *cfg, unsigned id, int value)
{
    if (!cfg) return 0;
    switch (id) {
#define XX(idx, type, key, def, opt, dec, lo, hi, step, exp1, exp2) \
        case idx: ESC_SET_##type(key, lo, hi);
        CFG_MAP(XX)
#undef XX
        default: return 0;
    }
}
#undef ESC_SET_val
#undef ESC_SET_str
