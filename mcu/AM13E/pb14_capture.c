/* ESCape32 Rel17 FW1 PB14 command input -- E62 HW Baseline v1.6.
 * PB14/GPIO46 is NOT a 5V tolerant MCU pin. E62 requires an external
 * bidirectional, level-compatible and contention-safe interface.
 *
 * Receive: GPIO46 -> INPUTXBAR1 -> ECAP0 (32bit timestamps).
 * eCAP captures START/END edges in continuous two-event mode.
 * Rel17 policy stays in src/io.c via original servo/DShot callbacks.
 * BiDShot reply waveform, DMA TX and RX->TX turnaround are NOT enabled;
 * no GPIO output is configured here. TX must meet TI Review/timing gate.
 *
 * ECAP counter frequency is measured against Rel17's 16 kHz SysTick
 * instead of assuming eCAP's clock divider. A valid 1ms measurement
 * is required before any throttle callback can execute.
 */
#include "pb14_capture.h"
#include "pb14_decode.h"
#include "io_backend.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_ecap.h>
#include <dl_xbar.h>
#include <stddef.h>

/* Original ESCape32 fatal policy; real motor shutdown remains unresolved. */
extern void hard_fault_handler(void);

#define PB14_GPIO        GPIO1
#define PB14_GPIO_PIN    DL_GPIO_PIN(14U)
#define PB14_PINCM       IOMUX_PINCM_PB14
#define PB14_GPIO_NUMBER 46U
#define PB14_ECAP       ECAP0

_Static_assert(IOMUX_PINCM_PB14 == 46, "E62 PB14/GPIO46 changed");

static AM13E_PB14_Decoder decoder;
static volatile uint32_t initialized;
static volatile uint32_t capture_pairs;
static volatile uint32_t unexpected_gpio1_irqs;
static uint32_t calib_counter;
static uint32_t calib_start;
static uint32_t calib_ticks_per_us;
static uint8_t inverted_rx;

static void input_fail_closed(void)
{
    __disable_irq();
    hard_fault_handler();
    for (;;) { __NOP(); }
}

void initio(void)
{
    DL_GPIO_enablePower(PB14_GPIO);
    if (!DL_GPIO_isPowerEnabled(PB14_GPIO)) input_fail_closed();
    /* Boot PB14 service relinquishes ownership. Keep output Hi-Z.
     * Do not enable internal pull-up/down: external 3.3/5-V interface
     * pull strength and idle levels are board design responsibilities.
     */
    DL_GPIO_disableOutput(PB14_GPIO, PB14_GPIO_PIN);
    DL_GPIO_initDigitalInput(PB14_PINCM);
    if (!DL_GPIO_isInputEnabled(PB14_PINCM) ||
        !DL_GPIO_isPeripheralConnected(PB14_PINCM) ||
        DL_GPIO_getPeripheralFunctionBits(PB14_PINCM) != IOMUX_PB14_GPIO46) {
        input_fail_closed();
    }
    /* Only ECAP0 uses INPUTXBAR1. No GPIO1 interrupt for PB14; GPIO1
     * remains reserved for existing PB15 nFAULT handling.
     */
    DL_GPIO_disableInterrupt(PB14_GPIO, PB14_GPIO_PIN);
    DL_GPIO_clearInterruptStatus(PB14_GPIO, PB14_GPIO_PIN);
    DL_XBAR_setInputXBAR(DL_XBAR_INPUT1, PB14_GPIO_NUMBER);
    if (INPUTXBAR->INPUTSELECT[DL_XBAR_INPUT1] != PB14_GPIO_NUMBER) {
        input_fail_closed();
    }

    /* Idle LOW = normal PWM/DShot, idle HIGH = inverted DShot.
     * This snapshot is NOT a substitute for the external idle-bias test.
     */
    inverted_rx = (DL_GPIO_readPins(PB14_GPIO, PB14_GPIO_PIN) != 0U);

    DL_ECAP_Config cap;
    DL_ECAP_initParamsSetDefault(&cap);
    cap.captureModeConfig.input = DL_ECAP_INPUT_INPUTXBAR1;
    cap.captureModeConfig.prescalerValue = 0U; /* no edge prescale */
    cap.captureModeConfig.continouousOrOneShot = DL_ECAP_CONTINUOUS_CAPTURE_MODE;
    cap.captureModeConfig.wrapOrStopAtEvent = DL_ECAP_EVENT_2;
    cap.captureModeConfig.captureEvent1Polarity =
        inverted_rx ? DL_ECAP_EVENT_FALLING_EDGE : DL_ECAP_EVENT_RISING_EDGE;
    cap.captureModeConfig.captureEvent2Polarity =
        inverted_rx ? DL_ECAP_EVENT_RISING_EDGE : DL_ECAP_EVENT_FALLING_EDGE;
    cap.captureModeConfig.resetCounter = true;
    cap.captureModeConfig.reArm = true;
    cap.interruptsConfig.interruptSourceEnableMask = DL_ECAP_ISR_SOURCE_CEVT2;
    DL_ECAP_init(PB14_ECAP, &cap);
    DL_ECAP_enableTimeStampCapture(PB14_ECAP);
    DL_ECAP_clearInterrupt(PB14_ECAP, DL_ECAP_ISR_SOURCE_CEVT2);
    DL_ECAP_clearGlobalInterrupt(PB14_ECAP);
    DL_ECAP_startCounter(PB14_ECAP);

    am13e_pb14_decoder_reset(&decoder, 0U, inverted_rx);
    calib_counter = 0U;
    calib_start = DL_ECAP_getTimeStampCounter(PB14_ECAP);
    initialized = 1U;
    /* Equal to 16 kHz SysTick priority (0): neither exception may
     * preempt the other while mutating the decoder.
     */
    NVIC_SetPriority(ECAP0_INT_IRQn, 0U);
    NVIC_EnableIRQ(ECAP0_INT_IRQn);
    /* Must NOT unmask global PRIMASK; motor safe-enable still pending. */
}

void ECAP0_IRQHandler(void)
{
    const uint16_t flags = DL_ECAP_getInterruptSource(PB14_ECAP);
    if (!initialized || (flags & ~DL_ECAP_ISR_SOURCE_CEVT2) != 0U) {
        input_fail_closed();
    }
    if (flags & DL_ECAP_ISR_SOURCE_CEVT2) {
        const uint32_t start = DL_ECAP_getEventTimeStamp(PB14_ECAP, DL_ECAP_EVENT_1);
        const uint32_t end = DL_ECAP_getEventTimeStamp(PB14_ECAP, DL_ECAP_EVENT_2);
        DL_ECAP_clearInterrupt(PB14_ECAP, DL_ECAP_ISR_SOURCE_CEVT2);
        DL_ECAP_clearGlobalInterrupt(PB14_ECAP);
        ++capture_pairs;
        /* Valid pulses alone may feed Rel17. No simulated throttle.
         * CRC and motor input watchdog remain in original src/io.c.
         */
        am13e_pb14_decoder_pulse(&decoder, start, end,
                                 am13e_app_io_servo_pulse,
                                 am13e_app_io_dshot_packet);
    } else {
        DL_ECAP_clearGlobalInterrupt(PB14_ECAP);
    }
}

/* Called by the real 16kHz TI SysTick vector only after GPIO + eCAP
 * initialization; 16 elapsed ticks = 1ms nominal at MCLK=200MHz.
 */
void am13e_app_pb14_systick(void)
{
    if (!initialized) return;
    if (++calib_counter == 16U) {
        const uint32_t now = DL_ECAP_getTimeStampCounter(PB14_ECAP);
        const uint32_t ticks = now - calib_start;
        calib_start = now;
        calib_counter = 0U;
        const uint32_t ticks_per_us = (ticks + 500U) / 1000U;
        /* Sanity only: expected ECAP <=200MHz and >=10MHz. Do not
         * infer or promise the precise clock before silicon validation.
         */
        if (ticks_per_us < 10U || ticks_per_us > 200U) input_fail_closed();
        calib_ticks_per_us = ticks_per_us;
        decoder.tick_hz = ticks_per_us * 1000000U;
    }
    if (calib_ticks_per_us) {
        am13e_pb14_decoder_idle(&decoder,
                                DL_ECAP_getTimeStampCounter(PB14_ECAP),
                                am13e_app_io_dshot_packet);
    }
}

void am13e_app_io_on_gpio1_interrupt(uint32_t pending)
{
    /* PB14 capture uses ECAP0, NOT GPIO1 IRQ. Unexpected enabled GPIO1
     * events are evidence of an IRQ ownership conflict and must not be
     * silently acknowledged as if a valid command were received.
     */
    ++unexpected_gpio1_irqs;
    (void)pending;
    input_fail_closed();
}

void am13e_app_pb14_status(AM13E_PB14_Status *out)
{
    if (out == NULL) return;
    out->capture_pairs = capture_pairs;
    out->good_pwm = decoder.good_pwm;
    out->good_dshot_rx = decoder.good_dshot;
    out->rejected = decoder.rejected;
    out->unexpected_gpio1_irqs = unexpected_gpio1_irqs;
    out->capture_ticks_per_us = calib_ticks_per_us;
    out->inverted_rx = inverted_rx;
    out->initialized = (uint8_t)initialized;
}
