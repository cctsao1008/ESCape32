/* Host integration exercising the actual rel17 prog.c translation unit. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "rel17_prog_host_shim.h"
#include "esc_param_metadata.h"

Cfg cfg = {.id = 1, .revision = 17, .revpatch = 0, .name = "E62"};
int throt, erpm, temp1, temp2, volt, curr, csum, beepval;
char analog, telphid = 1, rearm;

void checkcfg(void) { esc_checkcfg(&cfg); }
int savecfg(void) { return 1; }
int resetcfg(void) { return 1; }
int playmusic(const char *str, int vol) { (void)str; (void)vol; return 1; }
char *itoa(int value, char *out, int base)
{
    assert(base == 10);
    snprintf(out, 12, "%d", value);
    return out;
}
size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = 0;
    }
    return len;
}
int execcmd(char *str);
int execcrsfcmd(const char *buf, int len, char *res);

static void cli(const char *command, const char *expected)
{
    char buffer[4096] = {0};
    strlcpy(buffer, command, sizeof buffer);
    int n = execcmd(buffer);
    assert(n >= 0 && n < (int)sizeof buffer);
    buffer[n] = 0;
    if (strcmp(buffer, expected)) {
        fprintf(stderr, "CLI '%s': expected '%s', got '%s'\n", command, expected, buffer);
        assert(0);
    }
}
static void all_cli_parameter_reads(void)
{
    for (unsigned id = 0; id < esc_param_count(); ++id) {
        const char *name = esc_param_name(id);
        char command[64], response[4096];
        assert(name);
        int command_len = snprintf(command, sizeof command, "get %s", name);
        assert(command_len > 0 && command_len < (int)sizeof command);
        strcpy(response, command);
        int n = execcmd(response);
        assert(n > 0 && n < (int)sizeof response);
        response[n] = 0;
        char prefix[64];
        int prefix_len = snprintf(prefix, sizeof prefix, "%s: ", name);
        assert(prefix_len > 0 && prefix_len < (int)sizeof prefix);
        if (strncmp(response, prefix, (size_t)prefix_len) ||
            n < 4 || strcmp(response + n - 3, "OK\n")) {
            fprintf(stderr, "CLI read failed id=%u name=%s reply=%s\n",
                    id, name, response);
            assert(0);
        }
    }
}

static void all_crsf_parameter_reads(void)
{
    for (unsigned id = 0; id < esc_param_count(); ++id) {
        unsigned char response[512] = {0};
        const char frame[5] = {(char)0x2c, 0, 1, (char)(id + 1), 0};
        int n = execcrsfcmd(frame, sizeof frame, (char *)response);
        if (n <= 6 || response[0] != 0x2b || response[3] != id + 1) {
            fprintf(stderr, "CRSF read failed id=%u n=%d\n", id, n);
            assert(0);
        }
    }
}

int main(void)
{
    checkcfg();
    cli("set timing 19", "timing: 19\nOK\n");
    cli("get TIMING", "timing: 19\nOK\n");
    cli("set timing 99", "timing: 31\nOK\n");
    cli("set timing wrong", "ERROR\n");
    cli("get unknown", "ERROR\n");
    cli("set duty_min 4", "duty_min: 4\nOK\n");
    cli("get duty_min", "duty_min: 4\nOK\n");
    all_cli_parameter_reads();
    all_crsf_parameter_reads();

    unsigned char response[512] = {0};
    const char read_timing[] = {(char)0x2c, 0, 1, 5, 0};
    int n = execcrsfcmd(read_timing, sizeof read_timing, (char *)response);
    assert(n > 6 && response[0] == 0x2b);
    /* The rel17 CLI does not evaluate CFG_MAP exp2; CRSF does.
       Record the difference before making any behavioral change. */
    rearm = 0;
    cli("set throt_mode 1", "throt_mode: 1\\nOK\\n");
    assert(rearm == 0);
    const char write_mode[] = {(char)0x2d, 0, 1, 17, 1};
    n = execcrsfcmd(write_mode, sizeof write_mode, (char *)response);
    assert(n > 4);
    assert(cfg.throt_mode == 1 && rearm == 1);
    return 0;
}
