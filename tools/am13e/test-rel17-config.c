#include <assert.h>
#include <string.h>
#include "esc_config.h"

int main(void)
{
    Cfg cfg = {0};
    cfg.arm = 1;
    cfg.damp = 9;
    cfg.revdir = 2;
    cfg.timing = 99;
    cfg.freq_min = 2;
    cfg.freq_max = 1;
    cfg.duty_min = 0;
    cfg.duty_max = 1;
    cfg.throt_min = 500;
    cfg.throt_max = 600;
    cfg.throt_mid = 650;
    cfg.telem_poles = 5;
    cfg.prot_stall = 100;
    cfg.volume = 127;
    esc_checkcfg(&cfg);
    assert(cfg.arm == 1);
    assert(cfg.damp == 1 && cfg.revdir == 1);
    assert(cfg.timing == 31);
    assert(cfg.freq_min == 16 && cfg.freq_max == 16);
    assert(cfg.duty_min == 1 && cfg.duty_max == 1);
    assert(cfg.throt_min == 900);
    assert(cfg.throt_max == 1100);
    assert(cfg.throt_mid == 1000);
    assert(cfg.telem_poles == 4);
    assert(cfg.prot_stall == 1500);
    assert(cfg.volume == 100);
    Cfg once = cfg;
    esc_checkcfg(&cfg);
    assert(memcmp(&cfg, &once, sizeof cfg) == 0);
    return 0;
}
