/* ESCape32 Rel17 FW1 PB14 command input -- AM13E reference HW Baseline v1.6.
 * PB14/GPIO46 is NOT a 5V tolerant MCU pin. AM13E reference requires an external
 * bidirectional, level-compatible and contention-safe interface.
 *
 * Receive: GPIO46 -> INPUTXBAR1 -> ECAP0 (32bit timestamps).
 * eCAP captures two complete pulses per four-event group (CEVT4 IRQ).
 * Rel17 policy stays in src/io.c via original servo/DShot callbacks.
 * BiDShot RX->TIMG4/DMA TX->RX is integrated; waveform timing and
 * external bidirectional electrical interface remain hardware test gates.
 *
 * ECAP counter frequency is measured against Rel17's 16 kHz SysTick
 * instead of assuming eCAP's clock divider. A valid 1ms measurement
 * is required before any throttle callback can execute.
 */
#include "command_capture.h"
#include "command_decode.h"
#include "io_backend.h"
#include "input_watchdog.h"
#include "command_reply.h"
#include "command_input_route_backend.h"
#include "command_input_reference.h"
#include <soc.h>
#include <dl_gpio.h>
#include <dl_ecap.h>
#include <dl_dma.h>
#include <dl_xbar.h>
#include <stddef.h>

/* Original ESCape32 fatal policy; real motor shutdown remains unresolved. */
extern void hard_fault_handler(void);

/* AM13E230x TRM SPRUJF2B (rev B), Tables 11-3 and 24-4:
 *   DMA source index 39 = ECAP0DMA; 40 = ECAP1DMA.
 * The TI SDK 26.01.00.03 dl_dma.h enum has a one-instance naming
 * offset: ECAP1DMA=39, ECAP2DMA=40, despite device ECAP0/ECAP1 only.
 * Do NOT select 40 for PB14/ECAP0 simply by reading the SDK label.
 *
 * Future ECAP0 DMA integration must use source index 39, and must
 * separately prove the trigger routing, block completion, atomicity and
 * capture-overwrite behavior on silicon. No DMA is enabled in FW1 yet.
 */
#define AM13E_PB14_ECAP0_DMA_TRIGGER_INDEX 39U
_Static_assert((unsigned)DL_DMA_TRIGGER_SOURCE_ECAP1DMA ==
                   AM13E_PB14_ECAP0_DMA_TRIGGER_INDEX,
               "SDK ECAP DMA trigger map differs from AM13E230x TRM");
_Static_assert((unsigned)DL_DMA_TRIGGER_SOURCE_ECAP2DMA == 40U,
               "SDK second ECAP DMA trigger label or index changed");


/* ECFLG latches all four capture events despite only CEVT4 having
 * interrupt enabled; 32-bit TSCTR wrap is a normal counter event.
 * The group contains two pulses: CAP1->CAP2 and CAP3->CAP4.
 */
#define PB14_CAPTURE_GROUP_FLAGS (DL_ECAP_ISR_SOURCE_CEVT1 | \
                                  DL_ECAP_ISR_SOURCE_CEVT2 | \
                                  DL_ECAP_ISR_SOURCE_CEVT3 | \
                                  DL_ECAP_ISR_SOURCE_CEVT4)
#define PB14_ECAP_EXPECTED_FLAGS (PB14_CAPTURE_GROUP_FLAGS | \
                                  DL_ECAP_ISR_SOURCE_CTROVF)

/* Route is a board choice, not an AM13E silicon default. */
static const AM13E_CommandInputRoute reference_command_input = {
    .gpio = PB14_GPIO,
    .pin_mask = PB14_GPIO_PIN,
    .pincm = PB14_PINCM,
    .gpio_function = IOMUX_PB14_GPIO46,
    .gpio_index = PB14_GPIO_NUMBER,
    .input_xbar = PB14_INPUT_XBAR
};

static AM13E_PB14_Decoder decoder;
static volatile uint32_t initialized;
static volatile uint32_t capture_pairs;
static volatile uint32_t capture_overruns;
static volatile uint32_t late_capture_groups;
static volatile uint32_t max_capture_age_ticks;
static volatile uint32_t invalid_capture_groups;
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
    /* No PB15 interrupt-mask changes or unqualified output defaults. */
    if (!am13e_mcu_command_input_configure(&reference_command_input))
        input_fail_closed();

    /* Idle LOW = normal PWM/DShot, idle HIGH = inverted DShot.
     * This snapshot is NOT a substitute for the external idle-bias test.
     */
    inverted_rx = (DL_GPIO_readPins(PB14_GPIO, PB14_GPIO_PIN) != 0U);

    DL_ECAP_Config cap;
    DL_ECAP_initParamsSetDefault(&cap);
    cap.captureModeConfig.input = DL_ECAP_INPUT_INPUTXBAR1;
    cap.captureModeConfig.prescalerValue = 0U; /* no edge prescale */
    cap.captureModeConfig.continouousOrOneShot = DL_ECAP_CONTINUOUS_CAPTURE_MODE;
    /* Batch two pulses per ISR. DShot600 nominal IRQ rate is lowered
     * from ~600k/s to ~300k/s; measured capture reliability is pending.
     */
    cap.captureModeConfig.wrapOrStopAtEvent = DL_ECAP_EVENT_4;
    cap.captureModeConfig.captureEvent1Polarity =
        inverted_rx ? DL_ECAP_EVENT_FALLING_EDGE : DL_ECAP_EVENT_RISING_EDGE;
    cap.captureModeConfig.captureEvent2Polarity =
        inverted_rx ? DL_ECAP_EVENT_RISING_EDGE : DL_ECAP_EVENT_FALLING_EDGE;
    cap.captureModeConfig.captureEvent3Polarity =
        cap.captureModeConfig.captureEvent1Polarity;
    cap.captureModeConfig.captureEvent4Polarity =
        cap.captureModeConfig.captureEvent2Polarity;
    cap.captureModeConfig.resetCounter = true; /* init-only, not every edge */
    cap.captureModeConfig.reArm = true;
    cap.interruptsConfig.interruptSourceEnableMask = DL_ECAP_ISR_SOURCE_CEVT4;
    DL_ECAP_init(PB14_ECAP, &cap);
    DL_ECAP_enableTimeStampCapture(PB14_ECAP);
    DL_ECAP_clearInterrupt(PB14_ECAP, PB14_ECAP_EXPECTED_FLAGS);
    DL_ECAP_clearGlobalInterrupt(PB14_ECAP);
    DL_ECAP_startCounter(PB14_ECAP);

    am13e_pb14_decoder_reset(&decoder, 0U, inverted_rx);
    calib_counter = 0U;
    calib_start = DL_ECAP_getTimeStampCounter(PB14_ECAP);
    am13e_app_io_watchdog_prepare();
    am13e_pb14_bidir_tx_init();
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
    /* CEVT1..3 latch as normal data flags while only CEVT4 requests
     * an IRQ. CTROVF is legal for the free-running unsigned TSCTR.
     * A missing CEVT4 is not an actionable complete capture group.
     */
    if (!initialized || (flags & ~PB14_ECAP_EXPECTED_FLAGS) != 0U ||
        (flags & DL_ECAP_ISR_SOURCE_CEVT4) == 0U) {
        input_fail_closed();
    }
    /* Acknowledge OLD latched flags BEFORE reading CAP1..4. If any
     * CEVT1..4 event reappears during our snapshot it belongs to the
     * NEXT group and proves a racing capture/overwrite. Do not clear
     * those newly latched flags: the hardware must still complete the
     * next CEVT4 group and request its own IRQ.
     *
     * Unlike the earlier snapshot, checking phase alone could miss
     * a complete four-event wrap while registers were copied. The
     * new flag epoch catches events arriving after this ACK boundary.
     * A wrap entirely BEFORE the first ISR read is still unprovable
     * without DMA/oscilloscope timing evidence.
     */
    const DL_ECAP_EVENT phase_before =
        DL_ECAP_getModuloCounterStatus(PB14_ECAP);
    DL_ECAP_clearInterrupt(PB14_ECAP,flags & PB14_ECAP_EXPECTED_FLAGS);
    DL_ECAP_clearGlobalInterrupt(PB14_ECAP);
    const uint32_t start1=DL_ECAP_getEventTimeStamp(PB14_ECAP,DL_ECAP_EVENT_1);
    const uint32_t end1=DL_ECAP_getEventTimeStamp(PB14_ECAP,DL_ECAP_EVENT_2);
    const uint32_t start2=DL_ECAP_getEventTimeStamp(PB14_ECAP,DL_ECAP_EVENT_3);
    const uint32_t end2=DL_ECAP_getEventTimeStamp(PB14_ECAP,DL_ECAP_EVENT_4);
    const DL_ECAP_EVENT phase_after=
        DL_ECAP_getModuloCounterStatus(PB14_ECAP);
    const uint32_t flags_after=DL_ECAP_getInterruptSource(PB14_ECAP);
    if (!am13e_pb14_capture_snapshot_valid(
            flags,flags_after,PB14_CAPTURE_GROUP_FLAGS,
            (unsigned)phase_before,(unsigned)phase_after,
            (unsigned)DL_ECAP_EVENT_1)) {
        /* Quarantine partial/mixed groups and recover at the NEXT
         * complete CEVT4 event; never forward suspect throttle data.
         */
        ++capture_overruns;
        am13e_pb14_decoder_abort(&decoder);
        return;
    }
    if (!am13e_pb14_capture_group_valid(start1, end1, start2, end2,
                                        decoder.tick_hz)) {
        /* Invalid ordering can also indicate an overwritten capture
         * register. Do not deliver either pulse to the throttle logic.
         * A silent period >50ms will drop one capture pair by design.
         */
        ++invalid_capture_groups;
        am13e_pb14_decoder_abort(&decoder);
        return;
    }
    /* DShot150/300/600 arrives in two-pulse/four-edge groups.
     * CPU snapshot must finish before the NEXT CEVT4 can overwrite
     * these four CAP registers, even when flag/phase checks passed.
     * A deadline miss MUST NOT feed Rel17 throttle/WWDT/BiDShot.
     */
    const uint32_t snapshot_now=DL_ECAP_getTimeStampCounter(PB14_ECAP);
    const uint32_t age=snapshot_now-end2; /* modulo-2^32 rollover-safe */
    if(age>max_capture_age_ticks) max_capture_age_ticks=age;
    if(!am13e_pb14_capture_budget_ok(start1,start2,end2,
                                      snapshot_now,decoder.tick_hz)) {
        ++late_capture_groups;
        ++capture_overruns;
        am13e_pb14_decoder_abort(&decoder);
        return;
    }
    capture_pairs += 2U;
    const uint32_t good_before = decoder.good_dshot;
    /* Both pulses pass through the same Rel17 PWM/DShot callbacks.
     * No motor output or fake watchdog is introduced.
     */
    am13e_pb14_decoder_pulse(&decoder, start1, end1,
                             am13e_app_io_servo_pulse,
                             am13e_app_io_dshot_packet);
    am13e_pb14_decoder_pulse(&decoder, start2, end2,
                             am13e_app_io_servo_pulse,
                             am13e_app_io_dshot_packet);
    /* Reply to CRC-valid inverted DShot, anchored to the final edge. */
    if (inverted_rx && decoder.good_dshot != good_before) {
        (void)am13e_pb14_bidir_tx_start(end2, start2 - start1,
                                       decoder.tick_hz);
    }
}

/* Called by the real 16kHz TI SysTick vector only after GPIO + eCAP
 * initialization; 16 elapsed ticks = 1ms nominal at MCLK=200MHz.
 */
void am13e_app_pb14_systick(void)
{
    if (!initialized) return;
    am13e_pb14_bidir_tx_systick();
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

void am13e_pb14_resume_rx(void)
{
    if (!initialized) input_fail_closed();
    DL_ECAP_stopCounter(PB14_ECAP);
    DL_ECAP_resetCounters(PB14_ECAP);
    DL_ECAP_clearInterrupt(PB14_ECAP, PB14_ECAP_EXPECTED_FLAGS);
    DL_ECAP_clearGlobalInterrupt(PB14_ECAP);
    DL_ECAP_enableTimeStampCapture(PB14_ECAP);
    DL_ECAP_startCounter(PB14_ECAP);
    calib_start = DL_ECAP_getTimeStampCounter(PB14_ECAP);
    calib_counter = 0U;
    NVIC_ClearPendingIRQ(ECAP0_INT_IRQn);
    NVIC_EnableIRQ(ECAP0_INT_IRQn);
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
    out->capture_overruns = capture_overruns;
    out->late_capture_groups = late_capture_groups;
    out->max_capture_age_ticks = max_capture_age_ticks;
    out->invalid_capture_groups = invalid_capture_groups;
    out->good_pwm = decoder.good_pwm;
    out->good_dshot_rx = decoder.good_dshot;
    am13e_app_io_watchdog_status(&out->watchdog_armed,
                                &out->watchdog_valid_feeds);
    am13e_pb14_bidir_tx_status(&out->tx_dma_completed, &out->tx_rejected);
    out->rejected = decoder.rejected;
    out->unexpected_gpio1_irqs = unexpected_gpio1_irqs;
    out->capture_ticks_per_us = calib_ticks_per_us;
    out->inverted_rx = inverted_rx;
    out->initialized = (uint8_t)initialized;
}
