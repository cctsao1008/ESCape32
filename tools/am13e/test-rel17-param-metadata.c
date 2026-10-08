#include <assert.h>
#include <string.h>
#include "esc_param_metadata.h"
int main(void)
{
    static const char *const boundary[] = {
        "arm", "damp", "revdir", "brushed", "timing"
    };
    assert(esc_param_count() == 47);
    for (unsigned i = 0; i < esc_param_count(); ++i) {
        const char *name = esc_param_name(i);
        assert(name && *name);
        assert(esc_param_find(name) == (int)i);
        for (unsigned j = 0; j < i; ++j)
            assert(strcmp(name, esc_param_name(j)) != 0);
    }
    for (unsigned i = 0; i < 5; ++i)
        assert(strcmp(esc_param_name(i), boundary[i]) == 0);
    assert(strcmp(esc_param_name(46), "led") == 0);
    assert(esc_param_name(47) == NULL);
    assert(esc_param_find("TIMING") == 4);
    assert(esc_param_find("unknown") == 47);
    assert(esc_param_find(NULL) == -1);
    return 0;
}
