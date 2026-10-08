/* Portable parser primitives used by the ESCape32 rel17 command executor. */
#pragma once
int esc_cmd_getidx(const char *str, const char *const vec[]);
int esc_cmd_getval(const char *str, int *val);
int esc_cmd_getint(const char *buf, int len);
