/*
 * AM13E23019 PB14 service transport.
 *
 * E62 fixes the Boot service physical interface to PB14/GPIO46. The transport
 * preserves the upstream ESCape32 service characteristics:
 *
 *   38400 baud, 8 data bits, no parity, 1 stop bit, LSB first
 *   idle high
 *   500-ms receive timeout
 *
 * The implementation deliberately uses a small polling software-UART in Boot.
 * Runtime DShot/BiDShot remains a separate application-side timing problem.
 */

#include "boot_service_pb14.h"

#include <stddef.h>
#include <stdint.h>

#include "boot_platform.h"
#include "dl_common.h"
#include "dl_gpio.h"
#include "soc.h"

#define BOOT_SERVICE_PB14_IO_ID       0U
#define BOOT_SERVICE_PB14_BAUD        38400U
#define BOOT_SERVICE_PB14_TIMEOUT_MS  500U

#define BOOT_SERVICE_PB14_PINCM       IOMUX_PINCM_PB14
#define BOOT_SERVICE_PB14_PORT        GPIOB
#define BOOT_SERVICE_PB14_PIN         (1U << 14)

#define BOOT_SERVICE_PB14_BIT_CYCLES     ((BOOT_PLATFORM_MCLK_HZ + (BOOT_SERVICE_PB14_BAUD / 2U)) /         BOOT_SERVICE_PB14_BAUD)

#define BOOT_SERVICE_PB14_HALF_BIT_CYCLES     ((BOOT_SERVICE_PB14_BIT_CYCLES + 1U) / 2U)

#define BOOT_SERVICE_PB14_TIMEOUT_CYCLES     ((BOOT_PLATFORM_MCLK_HZ / 1000U) * BOOT_SERVICE_PB14_TIMEOUT_MS)

_Static_assert(BOOT_SERVICE_PB14_BIT_CYCLES > 0U,
    "PB14 bit time must be non-zero");
_Static_assert(BOOT_SERVICE_PB14_TIMEOUT_CYCLES <= 0x01000000U,
    "PB14 timeout must fit the 24-bit SysTick counter");

static void pb14_systick_stop(void)
{
    SysTick->CTRL = 0U;
}

static void pb14_systick_start(uint32_t cycles)
{
    SysTick->CTRL = 0U;
    SysTick->LOAD = cycles - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static bool pb14_systick_expired(void)
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0U;
}

static void pb14_delay_cycles(uint32_t cycles)
{
    pb14_systick_start(cycles);
    while (!pb14_systick_expired()) {
    }
    pb14_systick_stop();
}

static bool pb14_line_high(void)
{
    return DL_GPIO_readPins(
        BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN) != 0U;
}

static void pb14_release_to_rx(void)
{
    DL_GPIO_disableOutput(
        BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);

    DL_GPIO_initDigitalInputFeatures(
        BOOT_SERVICE_PB14_PINCM,
        DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_WAKEUP_DISABLE);
}

static void pb14_drive_tx(void)
{
    /*
     * Drive idle high before enabling the output driver to avoid a spurious
     * start bit during RX->TX turnaround.
     */
    DL_GPIO_setPins(
        BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);

    DL_GPIO_initDigitalOutputFeatures(
        BOOT_SERVICE_PB14_PINCM,
        DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_DRIVE_STRENGTH_LOW,
        DL_GPIO_HIZ_DISABLE);

    DL_GPIO_enableOutput(
        BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);
}

static bool pb14_wait_start_bit(void)
{
    pb14_systick_start(BOOT_SERVICE_PB14_TIMEOUT_CYCLES);

    while (pb14_line_high()) {
        if (pb14_systick_expired()) {
            pb14_systick_stop();
            return false;
        }
    }

    pb14_systick_stop();
    return true;
}

static int pb14_recv_buffer(char *buffer, int length)
{
    if ((buffer == NULL) || (length <= 0)) {
        return 0;
    }

    pb14_release_to_rx();

    for (int byte_index = 0; byte_index < length; ++byte_index) {
        if (!pb14_wait_start_bit()) {
            return 0;
        }

        /*
         * Confirm the start bit at its center. Falling-edge detection is
         * polling based; the half-bit delay centers subsequent samples.
         */
        pb14_delay_cycles(BOOT_SERVICE_PB14_HALF_BIT_CYCLES);
        if (pb14_line_high()) {
            return 0;
        }

        uint8_t value = 0U;

        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            pb14_delay_cycles(BOOT_SERVICE_PB14_BIT_CYCLES);
            if (pb14_line_high()) {
                value |= (uint8_t)(1U << bit);
            }
        }

        /* Sample the stop bit at its center; it must be high. */
        pb14_delay_cycles(BOOT_SERVICE_PB14_BIT_CYCLES);
        if (!pb14_line_high()) {
            return 0;
        }

        buffer[byte_index] = (char)value;
    }

    return 1;
}

static void pb14_send_buffer(const char *buffer, int length)
{
    if ((buffer == NULL) || (length <= 0)) {
        return;
    }

    pb14_drive_tx();

    for (int byte_index = 0; byte_index < length; ++byte_index) {
        uint8_t value = (uint8_t)buffer[byte_index];

        /* Start bit. */
        DL_GPIO_clearPins(
            BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);
        pb14_delay_cycles(BOOT_SERVICE_PB14_BIT_CYCLES);

        /* Eight data bits, LSB first. */
        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            if ((value & 0x01U) != 0U) {
                DL_GPIO_setPins(
                    BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);
            } else {
                DL_GPIO_clearPins(
                    BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);
            }

            value >>= 1;
            pb14_delay_cycles(BOOT_SERVICE_PB14_BIT_CYCLES);
        }

        /* Stop bit / idle. */
        DL_GPIO_setPins(
            BOOT_SERVICE_PB14_PORT, BOOT_SERVICE_PB14_PIN);
        pb14_delay_cycles(BOOT_SERVICE_PB14_BIT_CYCLES);
    }

    /*
     * Release the shared line after the final stop bit. The external
     * bidirectional front end must guarantee contention-safe turnaround.
     */
    pb14_release_to_rx();
}

static uint32_t pb14_device_id(void)
{
    /*
     * TI defines FACTORYREGION->DEVICEID as the device JTAG ID code. This is
     * the closest AM13E equivalent to the upstream STM32 DBGMCU_IDCODE field
     * used in the ESCape32 INFO response.
     */
    return FACTORYREGION->DEVICEID;
}

static const boot_service_transport_ops_t pb14_transport = {
    .io_id = BOOT_SERVICE_PB14_IO_ID,
    .recv_buffer = pb14_recv_buffer,
    .send_buffer = pb14_send_buffer,
    .device_id = pb14_device_id,
};

bool boot_service_pb14_init(void)
{
    /*
     * SYSRST leaves peripherals reset/disabled. Power GPIOB explicitly and
     * establish a safe idle-high input state before entering the service
     * receive window.
     */
    DL_GPIO_enablePower(BOOT_SERVICE_PB14_PORT);
    DL_Common_delayCycles(16U);

    pb14_release_to_rx();

    return true;
}

const boot_service_transport_ops_t *boot_service_pb14_transport(void)
{
    return &pb14_transport;
}
