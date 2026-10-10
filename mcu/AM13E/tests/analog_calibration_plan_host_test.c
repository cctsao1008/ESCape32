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
    puts("PASS: 4096 synthetic ADC values, 3.3V NTC norm + divider");
}
