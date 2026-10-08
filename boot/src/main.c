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

#include "common.h"

#define REVISION 4

#define CMD_PROBE  0
#define CMD_INFO   1
#define CMD_READ   2
#define CMD_WRITE  3
#define CMD_UPDATE 4
#define CMD_SETWRP 5

#define RES_OK    0
#define RES_ERROR 1

void main(void) {
	init();
	initio();
#if defined(AM13E)
	if (boot_am13e_take_reboot_ack()) {
		sendval(RES_OK);
	}
#else
	if (RCC_CSR & (RCC_CSR_SFTRSTF | RCC_CSR_OBLRSTF)) { // Reboot
		RCC_CSR = RCC_CSR_RMVF; // Clear reset flags
		sendval(RES_OK); // ACK after reboot
	}
#endif
#ifdef FAST_EXIT
	else goto done;
#endif
	for (;;) {
		switch (recvval()) {
			case CMD_PROBE: // Probe bootloader
				sendval(RES_OK);
				break;
			case CMD_INFO: { // Get info
#if defined(AM13E)
				uint32_t mcu = boot_am13e_device_id();
#else
				int mcu = DBGMCU_IDCODE;
#endif
#if defined(AM13E)
				char buf[32] = {REVISION, boot_am13e_io_id(), mcu, mcu >> 8, mcu >> 16, mcu >> 24};
#else
				char buf[32] = {REVISION, IO_PIN, mcu, mcu >> 8, mcu >> 16, mcu >> 24};
#endif
				senddata(buf, sizeof buf);
				break;
			}
			case CMD_READ: { // Read block
				int num = recvval();
				if (num == -1) goto done;
				int cnt = recvval();
				if (cnt == -1) goto done;
#if defined(AM13E)
				/* The MCU backend validates address arithmetic and readable Flash. */
				const void *read_addr = 0;
				const unsigned read_len = (unsigned)(cnt + 1) << 2;
				if (!boot_am13e_read_range((unsigned)num, read_len, &read_addr)) {
					sendval(RES_ERROR);
					break;
				}
				senddata(read_addr, (int)read_len);
#else
				senddata(_rom_end + (num << 10), (cnt + 1) << 2);
#endif
				break;
			}
			case CMD_WRITE: { // Write block
				int num = recvval();
				if (num == -1) goto done;
				char buf[1024];
				int len = recvdata(buf);
				if (len == -1) goto done;
#if defined(AM13E)
				/* Reject out-of-region or unaligned writes before touching Flash. */
				char *write_addr = 0;
				if (!boot_am13e_write_range((unsigned)num, (unsigned)len, &write_addr)) {
					sendval(RES_ERROR);
					break;
				}
				sendval(boot_am13e_flash_write(write_addr, buf, len) ? RES_OK : RES_ERROR);
#else
				sendval(write(_rom_end + (num << 10), buf, len) ? RES_OK : RES_ERROR);
#endif
				break;
			}
			case CMD_UPDATE: { // Update bootloader
#if defined(AM13E)
				/* Self-update must execute from a verified RAM-resident writer.
				 * The STM32 _rom/_ram_end buffering contract is not portable.
				 * Platform code must validate the whole image before touching boot Flash.
				 */
				/* Self-update is not enabled until RAM execution is qualified. */
				sendval(RES_ERROR);
				break;
#else
				char *buf = _ram_end; // Use upper SRAM as buffer
				int pos = 0;
				for (int i = 0, n = (_rom_end - _rom) >> 10; i < n; ++i) {
					int len = recvdata(buf + pos);
					if (len == -1) goto done;
					sendval(RES_OK);
					pos += len;
					if (len < 1024) break; // Last block
				}
				update(_rom, buf, pos);
				sendval(RES_ERROR);
				break;
#endif
			}
			case CMD_SETWRP: // Set write protection
#if defined(AM13E)
				if (recvval() == -1) goto done;
				/* Never acknowledge protection that was not applied. */
				sendval(RES_ERROR);
				break;
#else
				switch (recvval()) {
					case 0x33: // Off
						setwrp(0);
						break;
					case 0x44: // Bootloader
						setwrp(1);
						break;
					case 0x55: // Full
						setwrp(2);
						break;
				}
				sendval(RES_ERROR);
				break;
#endif
			default: // Pass control to application
			done:
#if defined(AM13E)
				/* Validate the AM13E application image and vector before launch.
				 * The hardware backend owns the actual memory and reset contract.
				 */
				if (!boot_am13e_application_valid()) break;
				boot_am13e_launch_application();
				__builtin_unreachable();
#else
				if (*(uint16_t *)_rom_end != 0x32ea) break;
				const uint32_t *vec = (const uint32_t *)(_rom_end + PAGE_SIZE); // Entry point
				__asm__ volatile (
					"msr msp, %0\n\t" // Initialize stack pointer
					"bx %1\n\t" // Jump to application
					:: "r" (vec[0]), "r" (vec[1]) : "memory");
				__builtin_unreachable();
#endif
		}
	}
}
