/* Host-only rel17 prog.c dependencies; no replacement parser or command logic. */
#pragma once
#ifndef ESCAPE32_PROG_HOST_TEST
#error "Host shim must never be used in firmware builds"
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esc_types.h"
#include "esc_config.h"
#include "esc_config_defaults.h"
#include "defs.h"

extern Cfg cfg;
extern int throt, erpm, temp1, temp2, volt, curr, csum, beepval;
extern char analog, telphid, rearm;
int savecfg(void);
int resetcfg(void);
int playmusic(const char *str, int vol);
void checkcfg(void);
char *itoa(int value, char *out, int base);
size_t strlcpy(char *dst, const char *src, size_t size);
static inline int min(int a, int b) { return a < b ? a : b; }
