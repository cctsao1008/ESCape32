/* E62 rel17 numeric parameter access. No motor, Flash, or command side effects. */
#pragma once
#include "esc_types.h"
int esc_param_get_numeric(const Cfg *cfg, unsigned id, int *value);
int esc_param_set_numeric(Cfg *cfg, unsigned id, int value);
