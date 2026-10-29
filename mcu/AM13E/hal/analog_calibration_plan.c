#include "analog_calibration_plan.h"
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

static int checked_product(uint64_t a,uint64_t b,uint64_t *out)
{
    if(b && a>UINT64_MAX/b)return 0;
    *out=a*b;return 1;
}
static int rounded_ratio(uint64_t n,uint64_t d,uint64_t *out)
{
    if(!d || n>UINT64_MAX-d/2U)return 0;
    *out=(n+d/2U)/d;return 1;
}
int am13e_bemf_reference_fresh(const AM13E_BemfReference *ref,uint32_t now)
{
    /* A reference is not publishable until a complete, uniquely counted
     * ADC epoch has arrived. All three timestamps belong to native
     * monotonic SysTick; uint32 deltas preserve legitimate clock wrap.
     * A stale scan, an unfinished scan and a future-dated completion
     * must all fail closed, even if acquisition_tick is recent.
     *
     * No guessed ADC/DAC board transfer function is introduced here.
     */
    if(!ref || !ref->sample || !ref->max_age_ticks ||
       ref->max_age_ticks>INT32_MAX)return 0;
    const uint32_t elapsed=(uint32_t)(now-ref->acquisition_tick);
    const uint32_t scan=(uint32_t)(ref->completion_tick-ref->acquisition_tick);
    const uint32_t since_complete=(uint32_t)(now-ref->completion_tick);
    return scan<=ref->max_age_ticks &&
           since_complete<=ref->max_age_ticks &&
           elapsed<=ref->max_age_ticks;
}
int am13e_bemf_reference_convert(const AM13E_BemfCalibration *c,
                                uint16_t raw,uint16_t code[3])
{
    if(!c || !code || !raw || raw>=4095U || !c->adc_span_uv ||
       !c->adc_gain_ppm || !c->bus_num || !c->bus_den || !c->vdda_uv ||
       !c->dac_gain_ppm || !c->bus_min_uv || c->bus_min_uv>c->bus_max_uv ||
       !c->max_age_ticks || c->max_age_ticks>INT32_MAX)return 0;
    const int64_t corrected=(int64_t)raw-c->adc_offset_code;
    uint64_t n,d,pin,bus;
    if(corrected<=0 || !checked_product((uint64_t)corrected,c->adc_span_uv,&n) ||
       !checked_product(n,1000000U,&n) ||
       !checked_product(4096U,c->adc_gain_ppm,&d) || !rounded_ratio(n,d,&pin) ||
       pin>UINT32_MAX)return 0;
    const int64_t sensed=(int64_t)pin+c->adc_low_uv-c->bus_offset_uv;
    if(sensed<=0 || !checked_product((uint64_t)sensed,c->bus_den,&n) ||
       !rounded_ratio(n,c->bus_num,&bus) ||
       bus<c->bus_min_uv || bus>c->bus_max_uv)return 0;
    uint16_t result[3];
    for(unsigned j=0;j<3U;++j){
        uint64_t phase,value;
        if(!c->phase_num[j] || !c->phase_den[j] ||
           !checked_product(bus,c->phase_num[j],&n) ||
           !rounded_ratio(n,(uint64_t)c->phase_den[j]*2U,&phase) ||
           phase>UINT32_MAX)return 0;
        const int64_t target=(int64_t)phase+c->phase_offset_uv[j]+
                             c->comparator_correction_uv[j];
        const int64_t adjusted=target-c->dac_offset_uv;
        if(target<=0 || (uint64_t)target>=c->vdda_uv || adjusted<=0 ||
           !checked_product((uint64_t)adjusted,4096U,&n) ||
           !checked_product(n,1000000U,&n) ||
           !checked_product(c->vdda_uv,c->dac_gain_ppm,&d) ||
           !rounded_ratio(n,d,&value) || value<=1U || value>4096U)return 0;
        result[j]=(uint16_t)(value-1U); /* TRM22.3: (D+1)*VDDA/4096 */
    }
    for(unsigned j=0;j<3U;++j)code[j]=result[j];
    return 1;
}

int am13e_adc_scale_pair(const AM13E_AdcCalibration *calibration,
                         uint16_t ntc_raw,uint16_t vbus_raw,
                         AM13E_AdcScaled *out)
{
    if(!calibration || !out || !calibration->adc_fullscale ||
       calibration->adc_fullscale>65535U ||
       !calibration->adc_vref_mv || calibration->adc_vref_mv>5000U ||
       !calibration->ntc_supply_mv || calibration->ntc_supply_mv>5000U ||
       !calibration->divider_bottom_ohms ||
       calibration->divider_top_ohms>10000000U ||
       calibration->divider_bottom_ohms>10000000U ||
       (uint32_t)ntc_raw>calibration->adc_fullscale ||
       (uint32_t)vbus_raw>calibration->adc_fullscale) return 0;
    const uint64_t fullscale=calibration->adc_fullscale;
    const uint64_t pin_mv=((uint64_t)ntc_raw*calibration->adc_vref_mv+
                            fullscale/2U)/fullscale;
    /* All original Rel17 NTC10K3455* functions take a 0..3300mV
     * voltage. If the board NTC divider is supplied at a different
     * reference, normalize only after using an explicit supply value.
     */
    const uint64_t ntc_mv=(pin_mv*UINT64_C(3300)+
                           calibration->ntc_supply_mv/2U)/
                           calibration->ntc_supply_mv;
    const uint64_t sum=(uint64_t)calibration->divider_top_ohms+
                       calibration->divider_bottom_ohms;
    const uint64_t denom=fullscale*
                        calibration->divider_bottom_ohms*UINT64_C(10);
    const uint64_t centivolts=((uint64_t)vbus_raw*
                     calibration->adc_vref_mv*sum+denom/2U)/denom;
    if(ntc_mv>3300U || centivolts>20000U) return 0;
    out->ntc_normalized_mv=(uint16_t)ntc_mv;
    out->vbus_centivolts=(uint16_t)centivolts;
    return 1;
}

int am13e_adc_receiver_millivolts(uint16_t raw,uint32_t fullscale,
                                   uint32_t vref_mv,uint16_t *out)
{
    if(!out || !fullscale || fullscale>UINT16_MAX ||
       !vref_mv || vref_mv>5000U || (uint32_t)raw>fullscale)
        return 0;
    *out=(uint16_t)(((uint64_t)raw*vref_mv+fullscale/2U)/fullscale);
    return 1;
}
