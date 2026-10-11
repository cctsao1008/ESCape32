/* Bounded, single-packet IRQ-owned TX FIFO feed state.
 * No fake UART registers, no blocking TX, no Physical qualification.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define AM13E_SERIAL_TX_MAX 64U
typedef struct {
    uint8_t data[AM13E_SERIAL_TX_MAX];
    unsigned count;
    unsigned sent;
    unsigned dropped;
    bool awaiting_eot;
} AM13E_SerialTxQueue;
bool am13e_serial_tx_queue_begin(AM13E_SerialTxQueue *q,
                                 const uint8_t *frame,unsigned bytes);
bool am13e_serial_tx_queue_next(AM13E_SerialTxQueue *q,uint8_t *byte);
bool am13e_serial_tx_queue_waiting_eot(const AM13E_SerialTxQueue *q);
void am13e_serial_tx_queue_eot(AM13E_SerialTxQueue *q);
