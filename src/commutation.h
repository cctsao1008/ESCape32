/* Original Rel17 nextstep() sequence. One Core-owned source of masks;
 * target adapters translate these masks, never choose a second sequence. */
#pragma once
#include <stdint.h>
static const uint16_t rel17_commutation_sequence[6]={0x175,0xd9,0x1ab,0x72,0x1de,0xac};
