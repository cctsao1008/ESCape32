/* AM13E23019 UART routes reviewed for this project, not a generic pinmux
 * table. PF/PINCM values come from TI SDK 26.01 hw_pinmap.h; bonding is
 * SPRSPC3A Table 5-2. Silicon validity does not establish PCB wiring.
 * Unknown packages and unreviewed routes fail closed. */
#pragma once
#include <hw_pinmap.h>
#define AM13E_UART_LQFP64_G 1
#define AM13E_UART_LQFP64_H 2
#define AM13E_UART_LQFP48 3
#define AM13E_UART_QFN48 4
#if !defined(AM13E_BOARD_MCU_PART) || !defined(AM13E_BOARD_PACKAGE)
#error "Native UART requires explicit AM13E23019 part and package"
#else
_Static_assert(AM13E_BOARD_MCU_PART==23019 &&
    AM13E_BOARD_PACKAGE>=AM13E_UART_LQFP64_G && AM13E_BOARD_PACKAGE<=AM13E_UART_QFN48,
    "Unsupported native UART device/package");
#ifdef AM13E_BOARD_ADC_PACKAGE_QFN48
_Static_assert((AM13E_BOARD_PACKAGE==AM13E_UART_QFN48)==AM13E_BOARD_ADC_PACKAGE_QFN48,
    "UART and ADC package declarations disagree");
#endif
#endif
/* Only reviewed pads: PA0/9/26 exist in all four packages; PA27 is
 * absent in 64H; PB10/11 absent in QFN48; PB18 only in 64G/64H. */
#define AM13E_UART_BONDED(p) ((p)==IOMUX_PINCM_PA0 || (p)==IOMUX_PINCM_PA9 || \
 (p)==IOMUX_PINCM_PA26 || ((p)==IOMUX_PINCM_PA27 && AM13E_BOARD_PACKAGE!=AM13E_UART_LQFP64_H) || \
 (((p)==IOMUX_PINCM_PB10 || (p)==IOMUX_PINCM_PB11) && AM13E_BOARD_PACKAGE!=AM13E_UART_QFN48) || \
 ((p)==IOMUX_PINCM_PB18 && AM13E_BOARD_PACKAGE<=AM13E_UART_LQFP64_H))
#define AM13E_UART_RX_ROUTE(u,p,f) ( \
 ((u)==0 && (p)==IOMUX_PINCM_PA26 && (f)==IOMUX_PA26_UC0_RX_SCL_SCLK) || \
 ((u)==3 && (p)==IOMUX_PINCM_PA26 && (f)==IOMUX_PA26_UC3_RX_SCL_SCLK) || \
 ((u)==0 && (p)==IOMUX_PINCM_PA27 && (f)==IOMUX_PA27_UC0_RX_SCL_SCLK) || \
 ((u)==3 && (p)==IOMUX_PINCM_PA27 && (f)==IOMUX_PA27_UC3_RX_SCL_SCLK) || \
 ((u)==0 && (p)==IOMUX_PINCM_PB18 && (f)==IOMUX_PB18_UC0_RX_SCL_SCLK) || \
 ((u)==3 && (p)==IOMUX_PINCM_PB18 && (f)==IOMUX_PB18_UC3_RX_SCL_SCLK) || \
 ((u)==3 && (p)==IOMUX_PINCM_PB10 && (f)==IOMUX_PB10_UC3_RX_SCL_SCLK) || \
 ((u)==1 && (p)==IOMUX_PINCM_PA9 && (f)==IOMUX_PA9_UC1_RX_SCL_SCLK))
#define AM13E_UART_TX_ROUTE(u,p,f) ( \
 ((u)==0 && (p)==IOMUX_PINCM_PA26 && (f)==IOMUX_PA26_UC0_TX_SDA_PICO) || \
 ((u)==3 && (p)==IOMUX_PINCM_PA26 && (f)==IOMUX_PA26_UC3_TX_SDA_PICO) || \
 ((u)==0 && (p)==IOMUX_PINCM_PA27 && (f)==IOMUX_PA27_UC0_TX_SDA_PICO) || \
 ((u)==3 && (p)==IOMUX_PINCM_PA27 && (f)==IOMUX_PA27_UC3_TX_SDA_PICO) || \
 ((u)==0 && (p)==IOMUX_PINCM_PB18 && (f)==IOMUX_PB18_UC0_TX_SDA_PICO) || \
 ((u)==3 && (p)==IOMUX_PINCM_PB18 && (f)==IOMUX_PB18_UC3_TX_SDA_PICO) || \
 ((u)==3 && (p)==IOMUX_PINCM_PB11 && (f)==IOMUX_PB11_UC3_TX_SDA_PICO) || \
 ((u)==1 && (p)==IOMUX_PINCM_PA0 && (f)==IOMUX_PA0_UC1_TX_SDA_PICO))
