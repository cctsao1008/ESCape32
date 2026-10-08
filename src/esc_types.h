/* ESCape32 rel17 shared firmware types; independent of MCU SDKs. */
#pragma once
#include <stdint.h>

typedef struct {
	const uint16_t id;
	const char revision;
	const char revpatch;
	const char name[15];
	const char _null;
	char arm;
	char damp;
	char revdir;
	char brushed;
	char timing;
	char sine_range;
	char sine_power;
	char freq_min;
	char freq_max;
	char duty_min;
	char duty_max;
	char duty_spup;
	char duty_ramp;
	char duty_rate;
	char duty_drag;
	char duty_lock;
	char throt_mode;
	char throt_rev;
	char throt_brk;
	char throt_set;
	char throt_ztc;
	char throt_cal;
	uint16_t throt_min;
	uint16_t throt_mid;
	uint16_t throt_max;
	uint16_t analog_min;
	uint16_t analog_max;
	char input_mode;
	char input_ch1;
	char input_ch2;
	char telem_mode;
	char telem_phid;
	char telem_poles;
	int16_t telem_volt;
	int16_t telem_curr;
	uint16_t prot_stall;
	char prot_temp;
	char prot_sens;
	char prot_volt;
	char prot_cells;
	uint16_t prot_curr;
	char prot_park;
	char music[256];
	char volume;
	char beacon;
	char bec;
	char led;
} Cfg;

typedef struct {
	int Kp, Ki, Kd, Li, i, x;
} PID;

