/* AM13E firmware command-layer declarations. Not a host test shim.
 * Command transport, persistent storage and audio are NOT implemented.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esc_types.h"
#include "esc_config.h"
#include "esc_config_defaults.h"

extern Cfg cfg;
extern int throt, erpm, temp1, temp2, volt, curr, csum, beepval;
extern char analog, telphid, rearm;

int savecfg(void);
int resetcfg(void);
int playmusic(const char *music, int volume);
void checkcfg(void);

char *am13e_prog_itoa(int value, char *out, int base);
char *am13e_prog_stpcpy(char *dst, const char *src);
char *am13e_prog_strsep(char **str, const char *sep);
size_t am13e_prog_strlcpy(char *dst, const char *src, size_t size);
static inline int min(int a, int b) { return a < b ? a : b; }

/* Name mapping stays restricted to prog.c via its selected port header. */
#define itoa am13e_prog_itoa
#define stpcpy am13e_prog_stpcpy
#define strsep am13e_prog_strsep
#define strlcpy am13e_prog_strlcpy
