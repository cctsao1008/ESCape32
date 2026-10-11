#pragma once
#include <soc.h>
#include <stdbool.h>
typedef int DL_UART_DIRECTION;
typedef struct { int clockSel,divideRatio; } DL_UART_ClockConfig;
typedef struct { int mode,direction,flowControl,parity,wordLength,stopBits; } DL_UART_Config;
enum { DL_UART_DIRECTION_RX,DL_UART_DIRECTION_TX_RX,DL_UART_MODE_NORMAL,
    DL_UART_FLOW_CONTROL_NONE,DL_UART_PARITY_NONE,DL_UART_PARITY_EVEN,
    DL_UART_WORD_LENGTH_8_BITS,DL_UART_STOP_BITS_ONE,DL_UART_STOP_BITS_TWO,
    DL_UART_CLOCK_BUSCLK,DL_UART_CLOCK_DIVIDE_RATIO_1,
    DL_UART_RX_FIFO_LEVEL_NOT_EMPTY,DL_UART_TX_FIFO_LEVEL_NOT_FULL };
typedef enum { DL_UART_IIDX_NO_INTERRUPT,DL_UART_IIDX_TX,DL_UART_IIDX_EOT_DONE,
    DL_UART_IIDX_OVERRUN_ERROR,DL_UART_IIDX_PARITY_ERROR,
    DL_UART_IIDX_FRAMING_ERROR,DL_UART_IIDX_NOISE_ERROR,
    DL_UART_IIDX_BREAK_ERROR,TEST_UART_RX } DL_UART_IIDX;
#define DL_UART_INTERRUPT_RX 1U
#define DL_UART_INTERRUPT_TX 2U
#define DL_UART_INTERRUPT_EOT_DONE 4U
#define DL_UART_INTERRUPT_OVERRUN_ERROR 8U
#define DL_UART_INTERRUPT_FRAMING_ERROR 16U
#define DL_UART_INTERRUPT_PARITY_ERROR 32U
#define DL_UART_INTERRUPT_NOISE_ERROR 64U
#define UNICOMMUART_RXDATA_DATA_MASK 255U
#define UNICOMMUART_RXDATA_FRMERR_SET 256U
#define UNICOMMUART_RXDATA_PARERR_SET 512U
#define UNICOMMUART_RXDATA_BRKERR_SET 1024U
#define UNICOMMUART_RXDATA_NERR_SET 2048U
void DL_UART_reset(TestUC *u);
void DL_UART_enablePower(TestUC *u);
bool DL_UART_isPowerEnabled(TestUC *u);
void DL_UART_setClockConfig(TestUC *u,const DL_UART_ClockConfig *c);
void DL_UART_init(TestUC *u,const DL_UART_Config *c);
void DL_UART_configBaudRate(TestUC *u,unsigned clock,unsigned baud);
void DL_UART_enableFIFOs(TestUC *u);
void DL_UART_setRXFIFOThreshold(TestUC *u,unsigned threshold);
void DL_UART_setTXFIFOThreshold(TestUC *u,unsigned threshold);
void DL_UART_clearInterruptStatus(TestUC *u,unsigned mask);
void DL_UART_enableInterrupt(TestUC *u,unsigned mask);
void DL_UART_disableInterrupt(TestUC *u,unsigned mask);
void DL_UART_enable(TestUC *u);
void DL_UART_disable(TestUC *u);
bool DL_UART_isRXFIFOEmpty(TestUC *u);
bool DL_UART_isTXFIFOFull(TestUC *u);
bool DL_UART_isBusy(TestUC *u);
DL_UART_IIDX DL_UART_getPendingInterrupt(TestUC *u);
void DL_UART_transmitData(TestUC *u,uint8_t byte);
