/* ESCape32 rel17 canonical parameter metadata. Keep IDs and ordering stable. */
#pragma once
#define CFG_MAP(XX) \
	XX( 0, val, arm,         ARM,         "off;on",                                        -1,    0,    1,  0, 0, 0) \
	XX( 1, val, damp,        DAMP,        "off;on",                                        -1,    0,    1,  0, 0, 0) \
	XX( 2, val, revdir,      REVDIR,      "off;on",                                        -1,    0,    1,  0, 0, 0) \
	XX( 3, val, brushed,     BRUSHED,     "off;on",                                        -1,    0,    1,  0, 0, 0) \
	XX( 4, val, timing,      TIMING,      "",                                               0,    1,   31,  0, cfg.brushed, 0) \
	XX( 5, val, sine_range,  SINE_RANGE,  "%",                                              0,    0,   25,  0, cfg.brushed, 0) \
	XX( 6, val, sine_power,  SINE_POWER,  "%",                                              0,    1,   15,  0, cfg.brushed, 0) \
	XX( 7, val, freq_min,    FREQ_MIN,    "kHz",                                            0,   16,   48,  0, cfg.brushed, 0) \
	XX( 8, val, freq_max,    FREQ_MAX,    "kHz",                                            0,   16,   96,  0, 0, 0) \
	XX( 9, val, duty_min,    DUTY_MIN,    "%",                                              0,    1,  100,  0, 0, 0) \
	XX(10, val, duty_max,    DUTY_MAX,    "%",                                              0,    1,  100,  0, 0, 0) \
	XX(11, val, duty_spup,   DUTY_SPUP,   "%",                                              0,    1,  100,  0, cfg.brushed, 0) \
	XX(12, val, duty_ramp,   DUTY_RAMP,   "kERPM",                                          0,    0,  100,  0, cfg.brushed, 0) \
	XX(13, val, duty_rate,   DUTY_RATE,   "%/ms",                                           1,    1,  100,  1, 0, 0) \
	XX(14, val, duty_drag,   DUTY_DRAG,   "%",                                              0,    0,  100,  0, 0, 0) \
	XX(15, val, duty_lock,   DUTY_LOCK,   "off;soft;hard",                                 -1,    0,    2,  0, cfg.brushed, 0) \
	XX(16, val, throt_mode,  THROT_MODE,  "fwd;fwd/rev;fwd/brk/rev;fwd/brk",               -1,    0,    3,  0, 0, rearm = 1) \
	XX(17, val, throt_rev,   THROT_REV,   "100%;75%;50%;25%",                              -1,    0,    3,  0, 0, 0) \
	XX(18, val, throt_brk,   THROT_BRK,   "%",                                              0,    0,  100,  0, 0, 0) \
	XX(19, val, throt_set,   THROT_SET,   "%",                                              0,    0,  100,  0, cfg.arm, 0) \
	XX(20, val, throt_ztc,   THROT_ZTC,   "off;on",                                        -1,    0,    1,  0, cfg.brushed, 0) \
	XX(21, val, throt_cal,   THROT_CAL,   "off;on",                                        -1,    0,    1,  0, 0, 0) \
	XX(22, val, throt_min,   THROT_MIN,   "us",                                             0,  900, 1900, 10, 0, 0) \
	XX(23, val, throt_mid,   THROT_MID,   "us",                                             0, 1000, 2000, 10, 0, 0) \
	XX(24, val, throt_max,   THROT_MAX,   "us",                                             0, 1100, 2100, 10, 0, 0) \
	XX(25, val, analog_min,  ANALOG_MIN,  "mV",                                             0,    0, 3200, 10, 0, 0) \
	XX(26, val, analog_max,  ANALOG_MAX,  "mV",                                             0,  200, 3400, 10, 0, 0) \
	XX(27, val, input_mode,  INPUT_MODE,  "servo;analog;serial;iBUS;SBUS;CRSF;EXBUS;HoTT", -1,    0,    7,  0, 0, rearm = 1) \
	XX(28, val, input_ch1,   INPUT_CH1,   "",                                               0,    0,   32,  0, 0, rearm = 1) \
	XX(29, val, input_ch2,   INPUT_CH2,   "",                                               0,    0,   32,  0, 0, 0) \
	XX(30, val, telem_mode,  TELEM_MODE,  "KISS;KISS auto;iBUS;S.Port;CRSF;MSB;HoTT",      -1,    0,    6,  0, 0, 0) \
	XX(31, val, telem_phid,  TELEM_PHID,  "",                                               0,    0,   28,  0, 0, 0) \
	XX(32, val, telem_poles, TELEM_POLES, "",                                               0,    2,  100,  0, 0, 0) \
	XX(33, val, telem_volt,  TELEM_VOLT,  "",                                               0,  -80,  160,  0, 0, 0) \
	XX(34, val, telem_curr,  TELEM_CURR,  "",                                               0, -100,  200,  0, 0, 0) \
	XX(35, val, prot_stall,  PROT_STALL,  "ERPM",                                           0,    0, 3500, 10, cfg.brushed, 0) \
	XX(36, val, prot_temp,   PROT_TEMP,   "C",                                              0,    0,  140,  5, 0, 0) \
	XX(37, val, prot_sens,   PROT_SENS,   "ESC;motor;both",                                -1,    0,    2,  0, 0, 0) \
	XX(38, val, prot_volt,   PROT_VOLT,   "V",                                              1,    0,   38,  1, 0, 0) \
	XX(39, val, prot_cells,  PROT_CELLS,  "",                                               0,    0,   24,  0, 0, 0) \
	XX(40, val, prot_curr,   PROT_CURR,   "A",                                              0,    0,  999,  0, 0, 0) \
	XX(41, val, prot_park,   PROT_PARK,   "",                                               0,    0,    4,  0, 0, 0) \
	XX(42, str, music,       MUSIC,,,,,                                                                     0, 0, 0) \
	XX(43, val, volume,      VOLUME,      "%",                                              0,    0,  100,  0, 0, 0) \
	XX(44, val, beacon,      BEACON,      "%",                                              0,    0,  100,  0, 0, 0) \
	XX(45, val, bec,         BEC,         "5.5V;6.5V;7.4V;8.4V;12V",                       -1,    0,    4,  0, 0, 0) \
	XX(46, val, led,         LED,         "",                                               0,    0,   15,  0, 0, 0) \

#define PARAM_CNT 47

