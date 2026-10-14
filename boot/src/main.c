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
#if defined(AM13E)
#define CMD_WINDOW 6 /* Rev1.6: Config-prefix stream, 492 logical 1KiB blocks */
#include "../../mcu/AM13E/hal/flash_partition.h"
#include "../mcu/AM13E/hal/update_staging.h"
#include "update_commit.h"
#endif

#define RES_OK    0
#define RES_ERROR 1

#if defined(AM13E)
/* The TI Reset_Handler calls int main(void) in boot/mcu/AM13E/config.c.
 * Rel17's command dispatcher remains a void function, without a
 * duplicate vector/Reset_Handler or a new protocol implementation.
 */
void am13e_rel17_boot_main(void);
void am13e_rel17_boot_main(void) {
#else
void main(void) {
#endif
#if defined(AM13E)
    /* Boot entry/reboot session always begins at legacy window 0.
     * Host packaging must account for the new 4KiB Config prefix.
     */
    unsigned app_window=0U;
#endif
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
                const unsigned read_len=(unsigned)(cnt+1)<<2;
                const unsigned block=(app_window<<8U)|(unsigned)num;
                /* Rev1.6 invalid READ sends a value error and stays in
                 * programming mode, even with an eligible Application.
                 */
                if (!boot_am13e_read_range(block,read_len,&read_addr)) {
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
				const unsigned block=(app_window<<8U)|(unsigned)num;
                if (!boot_am13e_write_range(block,(unsigned)len,&write_addr)) {
					sendval(RES_ERROR);
					break;
				}
				sendval(write(write_addr, buf, len) ? RES_OK : RES_ERROR);
#else
				sendval(write(_rom_end + (num << 10), buf, len) ? RES_OK : RES_ERROR);
#endif
				break;
			}
			#if defined(AM13E)
            case CMD_WINDOW: {
                /* Complement-coded one-byte selector, same recvval()
                 * framing as CMD_READ/WRITE arguments.
                 * Unsupported values never change the active window.
                 */
                const int selected=recvval();
                if(selected<0) goto done;
                if(selected>1) {
                    sendval(RES_ERROR);
                    break;
                }
                app_window=(unsigned)selected;
                sendval(RES_OK);
                break;
            }
#endif
            case CMD_UPDATE: { // Update bootloader
#if defined(AM13E)
                /* Preserve Rel17 recvdata(), CRC and per-frame ACKs.
                 * Stage only: no Boot Bank0 erase/program/reset until
                 * SRAM-only execution and on-target recovery qualified.
                 */
                boot_am13e_stage_begin();
                for(unsigned i=0U;i<AM13E_BOOT_STAGE_BLOCKS;++i) {
                    uint8_t *dst=boot_am13e_stage_next(i);
                    if(!dst) {
                        boot_am13e_stage_abort();
                        goto done;
                    }
                    int len=recvdata((char *)dst);
                    if(len<0 || !boot_am13e_stage_accept(i,(unsigned)len)) {
                        boot_am13e_stage_abort();
                        goto done;
                    }
                    sendval(RES_OK);
                    if(len<(int)AM13E_BOOT_STAGE_BLOCK_BYTES)break;
                }
                /* A separately qualified build may perform SRAM_C
                 * self-update. Reference code ALWAYS aborts and NAKs.
                 */
#if defined(AM13E_BOOT_RECOVERY_QUALIFIED) && AM13E_BOOT_RECOVERY_QUALIFIED
                if(boot_am13e_stage_vector_plausible())
                    boot_am13e_update_commit_quarantined();
#endif
                boot_am13e_stage_abort();
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
				const int requested=recvval();
				if(requested<0)goto done;
				/* Three original Rel17 selectors; default setwrp returns. */
				if(requested==0x33)setwrp(0);
				else if(requested==0x44)setwrp(1);
				else if(requested==0x55)setwrp(2);
				/* No fake success before a verified reset. */
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
