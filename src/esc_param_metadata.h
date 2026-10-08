#pragma once
#include "esc_param_map.h"
unsigned esc_param_count(void);
const char *esc_param_name(unsigned id);
int esc_param_find(const char *name);
