#include <assert.h>
#include "esc_param_access.h"
#include "esc_param_metadata.h"

int main(void)
{
    Cfg cfg = {0};
    int v = -1;
    assert(esc_param_set_numeric(&cfg, 4, 19));
    assert(esc_param_get_numeric(&cfg, 4, &v) && v == 19);
    assert(esc_param_set_numeric(&cfg, 7, 24));
    assert(esc_param_set_numeric(&cfg, 8, 48));
    assert(esc_param_get_numeric(&cfg, 8, &v) && v == 48);
    assert(esc_param_set_numeric(&cfg, 22, 1000));
    assert(esc_param_set_numeric(&cfg, 24, 2000));
    assert(esc_param_set_numeric(&cfg, 23, 1500));
    assert(esc_param_get_numeric(&cfg, 23, &v) && v == 1500);
    assert(esc_param_set_numeric(&cfg, 33, -50) == 1);
    assert(esc_param_get_numeric(&cfg, 33, &v) && v == 0); /* no sensor */
    assert(!esc_param_set_numeric(&cfg, 4, 32));
    assert(esc_param_get_numeric(&cfg, 4, &v) && v == 19);
    assert(!esc_param_set_numeric(&cfg, 42, 12)); /* music is a string */
    assert(!esc_param_get_numeric(&cfg, 42, &v));
    assert(!esc_param_set_numeric(&cfg, 47, 1));
    assert(!esc_param_get_numeric(&cfg, 47, &v));
    assert(!esc_param_set_numeric(0, 4, 16));
    assert(!esc_param_get_numeric(0, 4, &v));
    assert(!esc_param_get_numeric(&cfg, 4, 0));
    assert(esc_param_find("TIMING") == 4);
    return 0;
}
