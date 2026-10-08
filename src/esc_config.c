/* ESCape32 rel17 configuration normalization, independent of Flash I/O. */
#include "esc_config.h"
#include "defs.h"

static inline int cfg_min(int a, int b) { return a < b ? a : b; }
static inline int cfg_max(int a, int b) { return a > b ? a : b; }
static inline int clamp(int x, int a, int b)
{
    return cfg_min(cfg_max(x, a), b);
}

#ifdef ANALOG
#define IO_ANALOG (cfg->throt_set < 100)
#elif defined(IO_PA2) || defined(IO_PA6) || defined(ANALOG_CHAN)
#define IO_ANALOG (cfg->input_mode == 1)
#else
#define IO_ANALOG 0
#endif

void esc_checkcfg(Cfg *cfg) {
#ifndef ANALOG
#ifndef ANALOG_CHAN
	if (IO_ANALOG) cfg->arm = 1; // Ensure low level on startup
	else
#endif
	cfg->arm = !!cfg->arm;
#else
	cfg->arm = 0;
#endif
#ifdef PWM_ENABLE
	cfg->damp = 1;
#else
	cfg->damp = !!cfg->damp;
#endif
	cfg->revdir = !!cfg->revdir;
	cfg->brushed = !!cfg->brushed;
	cfg->timing = clamp(cfg->timing, 1, 31);
	cfg->sine_range = cfg->sine_range && !cfg->brushed ? clamp(cfg->sine_range, 5, 25) : 0;
	cfg->sine_power = clamp(cfg->sine_power, 1, 15);
	cfg->freq_min = clamp(cfg->freq_min, 16, 48);
	cfg->freq_max = clamp(cfg->freq_max, cfg->freq_min, 96);
	cfg->duty_min = clamp(cfg->duty_min, 1, 100);
	cfg->duty_max = clamp(cfg->duty_max, cfg->duty_min, 100);
	cfg->duty_spup = clamp(cfg->duty_spup, 1, 100);
	cfg->duty_ramp = clamp(cfg->duty_ramp, 0, 100);
	cfg->duty_rate = clamp(cfg->duty_rate, 1, 100);
	cfg->duty_drag = clamp(cfg->duty_drag, 0, 100);
	cfg->duty_lock = clamp(cfg->duty_lock, 0, cfg->brushed ? 0 : 2);
	cfg->throt_mode = clamp(cfg->throt_mode, 0, IO_ANALOG ? 0 : cfg->duty_lock ? 1 : 3);
	cfg->throt_rev = clamp(cfg->throt_rev, 0, 3);
	cfg->throt_brk = clamp(cfg->throt_brk, cfg->duty_drag, 100);
	cfg->throt_set = clamp(cfg->throt_set, 0, cfg->arm ? 0 : 100);
	cfg->throt_ztc = !!cfg->throt_ztc;
	cfg->throt_cal = !!cfg->throt_cal;
	cfg->throt_min = clamp(cfg->throt_min, 900, 1900);
	cfg->throt_max = clamp(cfg->throt_max, cfg->throt_min + 200, 2100);
	cfg->throt_mid = clamp(cfg->throt_mid, cfg->throt_min + 100, cfg->throt_max - 100);
	cfg->analog_min = clamp(cfg->analog_min, 0, 3200);
	cfg->analog_max = clamp(cfg->analog_max, cfg->analog_min + 200, 3400);
#ifdef IO_PA2
	cfg->input_mode = clamp(cfg->input_mode, 0, 7);
#ifdef DISABLE_EXBUS
	if (cfg->input_mode == 6) cfg->input_mode = 0;
#endif
#ifdef DISABLE_HOTT
	if (cfg->input_mode == 7) cfg->input_mode = 0;
#endif
	cfg->input_ch1 = clamp(cfg->input_ch1, 1, cfg->input_mode < 3 ? 0 : 32);
	cfg->input_ch2 = clamp(cfg->input_ch2, 0, cfg->input_mode < 3 ? 0 : 32);
#else
#if defined IO_PA6 || defined ANALOG_CHAN
	cfg->input_mode = clamp(cfg->input_mode, 0, 1);
#else
	cfg->input_mode = 0;
#endif
	cfg->input_ch1 = 0;
	cfg->input_ch2 = 0;
#endif
	cfg->telem_mode = clamp(cfg->telem_mode, 0, 6);
#ifdef DISABLE_MSB
	if (cfg->telem_mode == 5) cfg->telem_mode = 0;
#endif
#ifdef DISABLE_HOTT
	if (cfg->telem_mode == 6) cfg->telem_mode = 0;
#endif
	cfg->telem_phid =
		cfg->telem_mode == 2 ||
		cfg->telem_mode == 5 ? clamp(cfg->telem_phid, 1, 2):
		cfg->telem_mode == 3 ? clamp(cfg->telem_phid, 1, 28):
		cfg->telem_mode == 4 ? clamp(cfg->telem_phid, 1, 8):
		cfg->input_mode == 4 ? clamp(cfg->telem_phid, 0, 4) : 0;
	cfg->telem_poles = clamp(cfg->telem_poles & ~1, 2, 100);
#if SENS_CNT >= 1
	cfg->telem_volt = clamp(cfg->telem_volt, -80, 160);
#else
	cfg->telem_volt = 0;
#endif
#if SENS_CNT >= 2
	cfg->telem_curr = clamp(cfg->telem_curr, -100, 200);
#else
	cfg->telem_curr = 0;
#endif
	cfg->prot_stall = cfg->prot_stall && !cfg->brushed ? clamp(cfg->prot_stall, 1500, 3500) : 0;
	cfg->prot_temp = cfg->prot_temp ? clamp(cfg->prot_temp, 60, 140) : 0;
#if SENS_CNT >= 3
	cfg->prot_sens = clamp(cfg->prot_sens, 0, 2);
#else
	cfg->prot_sens = 0;
#endif
#if SENS_CNT >= 1 && !defined ANALOG
	cfg->prot_volt = cfg->prot_volt ? clamp(cfg->prot_volt, 28, 38) : 0;
	cfg->prot_cells = clamp(cfg->prot_cells, 0, 24);
#else
	cfg->prot_volt = 0;
	cfg->prot_cells = 0;
#endif
#if SENS_CNT >= 2
	cfg->prot_curr = clamp(cfg->prot_curr, 0, 999);
#else
	cfg->prot_curr = 0;
#endif
#ifdef PARK_PIN
	cfg->prot_park = clamp(cfg->prot_park, 0, 4);
#else
	cfg->prot_park = 0;
#endif
	cfg->volume = clamp(cfg->volume, 0, 100);
	cfg->beacon = clamp(cfg->beacon, 0, 100);
#ifdef BEC_MAP
	cfg->bec = clamp(cfg->bec, BEC_MIN, BEC_MAX);
#else
	cfg->bec = 0;
#endif
	cfg->led &= (1 << LED_CNT) - 1;
}

