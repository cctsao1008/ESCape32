/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This firmware is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this firmware. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#if defined(AM13E)
/* AM13E peripheral declarations belong to the MCU boot backend. */
#else
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#ifdef AT32F4
#include <libopencm3/cm3/common.h>
#include <libopencm3/stm32/f1/usart.h>
#else
#include <libopencm3/stm32/usart.h>
#endif
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/dbgmcu.h>
#endif
#include "config.h"

#if defined(AM13E)
/* Pin identifiers and peripheral clock conversion are backend-owned. */
#ifndef IO_PB14
#error "AM13E boot target requires an explicit service I/O pin"
#endif
#else
#define CLK_CNT(rate) ((CLK + ((rate) >> 1)) / (rate))

#ifdef IO_PA2
#define IO_PIN 1
#elif defined IO_PA6
#define IO_PIN 2
#define TIM3_IDR (GPIOA_IDR & 0x40) // A6
#else
#define IO_PIN 3
#define TIM3_IDR (GPIOB_IDR & 0x10) // B4
#endif

#endif

extern char _rom[], _rom_end[], _ram_end[]; // Linker exports

void init(void);
void initio(void);

int recvbuf(char *buf, int len);
void sendbuf(const char *buf, int len);
int recvval(void);
void sendval(int val);
int recvdata(char *buf);
void senddata(const char *buf, int len);

uint32_t crc32(const char *buf, int len);
/* Keep the original Rel17 write() interface, including on AM13E.
 * AM13E Flash authorization remains APP-only until a separately
 * qualified original Boot self-update implementation is available.
 */
#if defined(AM13E)
int write(char *dst, const char *src, int len);
#else
int write(char *dst, const char *src, int len) __attribute__((__long_call__));
void update(char *dst, const char *src, int len) __attribute__((__long_call__));
#endif
void setwrp(int type);

#if defined(AM13E)
/* Required MCU backend hooks; no dummy implementations. */
bool boot_am13e_take_reboot_ack(void);
uint32_t boot_am13e_device_id(void);
uint8_t boot_am13e_io_id(void);
/* Resolve only validated Flash ranges, including overflow/alignment checks. */
bool boot_am13e_read_range(unsigned block, unsigned length, const void **address);
bool boot_am13e_write_range(unsigned block, unsigned length, char **address);
bool boot_am13e_application_valid(void);
__attribute__((noreturn)) void boot_am13e_launch_application(void);
#endif
