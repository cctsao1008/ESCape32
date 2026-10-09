#pragma once
#include <stdint.h>
#include "bidir_codec.h"
#define AM13E_BIDIR_DMA_TRANSFERS AM13E_BIDIR_DMA_LEVELS
uint32_t am13e_bidir_tx_period_ticks(uint32_t rx_ticks,uint32_t rx_hz,uint32_t timer_hz);
void am13e_bidir_toggle_plan(const uint8_t levels[AM13E_BIDIR_DMA_LEVELS],
                            uint32_t pin_mask,uint32_t words[AM13E_BIDIR_DMA_TRANSFERS]);
