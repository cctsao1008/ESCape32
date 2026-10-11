#include "analog_calibration_plan.h"
#include <stddef.h>
#include <stdint.h>

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
