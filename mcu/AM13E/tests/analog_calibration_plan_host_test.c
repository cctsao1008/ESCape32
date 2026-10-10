/* Pure arithmetic / unit contract, NOT a measured AM13E reference NTC or divider. */
#include "analog_calibration_plan.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const AM13E_AdcCalibration fixture={4095U,3300U,3300U,
                                        100000U,10000U};
    AM13E_AdcScaled out;
    assert(!am13e_adc_scale_pair(NULL,2048U,2048U,&out));
    assert(!am13e_adc_scale_pair(&fixture,4096U,1U,&out));
    assert(!am13e_adc_scale_pair(&fixture,1U,4096U,&out));
    assert(am13e_adc_scale_pair(&fixture,0U,0U,&out));
    assert(out.ntc_normalized_mv==0U && out.vbus_centivolts==0U);
    assert(am13e_adc_scale_pair(&fixture,2048U,2048U,&out));
    assert(out.ntc_normalized_mv>=1649U && out.ntc_normalized_mv<=1652U);
    assert(out.vbus_centivolts>=1814U && out.vbus_centivolts<=1817U);
    for(uint32_t code=0;code<=4095U;++code){
        assert(am13e_adc_scale_pair(&fixture,(uint16_t)code,1000U,&out));
        assert(out.ntc_normalized_mv<=3300U);
    }
    /* Board-provided analog receiver VREF must feed the *existing*
     * Rel17 adcdata(...,a) in millivolts; it is not NTC/VBUS.
     */
    uint16_t mv=0U;
    assert(!am13e_adc_receiver_millivolts(0U,0U,3300U,&mv));
    assert(!am13e_adc_receiver_millivolts(0U,4095U,0U,&mv));
    assert(!am13e_adc_receiver_millivolts(4096U,4095U,3300U,&mv));
    assert(!am13e_adc_receiver_millivolts(1U,4095U,5001U,&mv));
    assert(!am13e_adc_receiver_millivolts(1U,4095U,3300U,NULL));
    assert(am13e_adc_receiver_millivolts(0U,4095U,3300U,&mv) && mv==0U);
    assert(am13e_adc_receiver_millivolts(4095U,4095U,3300U,&mv) && mv==3300U);
    unsigned previous=0U;
    for(uint32_t adc=0;adc<=4095U;++adc) {
        assert(am13e_adc_receiver_millivolts((uint16_t)adc,4095U,3300U,&mv));
        assert((unsigned)mv>=previous && mv<=3300U);
        previous=mv;
    }
    puts("PASS: Analog receiver ADC0 SOC2 millivolt conversion (4096 codes)");
    puts("PASS: 4096 synthetic ADC values, 3.3V NTC norm + divider");
}
