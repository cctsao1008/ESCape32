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

#include "device.h"
#include <dl_crcp.h>

/* The ESCape32 boot protocol uses a reflected CRC-32 with all-one seed
 * and final inversion. CRCP peripheral initialization is explicit here;
 * low-level clock/power availability belongs to the AM13E boot init.
 * CRCP result equivalence still requires native/on-target verification.
 */
uint32_t crc32(const char *buf, int len) {
    if (!buf || len < 0 || (len & 3)) return 0;
    DL_CRCP_enablePower(CRC);
    DL_CRCP_init(CRC, DL_CRCP_POLYNOMIAL_SIZE_32,
                 DL_CRCP_BIT_REVERSED,
                 DL_CRCP_INPUT_ENDIANESS_LITTLE_ENDIAN,
                 DL_CRCP_OUTPUT_BYTESWAP_DISABLED);
    DL_CRCP_setPolynomial(CRC, UINT32_C(0x04c11db7));
    DL_CRCP_setSeed32(CRC, UINT32_C(0xffffffff));
    for (int i = 0; i < len; ++i)
        DL_CRCP_feedData8(CRC, (uint8_t)buf[i]);
    return ~DL_CRCP_getResult32(CRC);
}
