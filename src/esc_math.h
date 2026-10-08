/* Shared ESCape32 rel17 arithmetic and CRC API; no MCU dependencies. */
#pragma once
#include <stdint.h>
#include "esc_types.h"

uint8_t crc8(const char *buf, int len);
uint8_t crc8dvbs2(const char *buf, int len);
uint16_t crc16ccitt(const char *buf, int len);
uint16_t crc16xmodem(const char *buf, int len);
int scale(int x, int a1, int a2, int b1, int b2);
int smooth(int *s, int x, int n);
void initpid(PID *pid, int x);
int calcpid(PID *pid, int x, int y);
