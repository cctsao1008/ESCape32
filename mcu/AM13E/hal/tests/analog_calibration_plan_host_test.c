/* Pure arithmetic / unit contract, NOT a measured AM13E reference NTC or divider. */
#include "analog_calibration_plan.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static void sc5_conversion(void)
{
    AM13E_BemfCalibration c={
        .adc_span_uv=2500000U,.adc_gain_ppm=1000000U,
        .bus_num=1U,.bus_den=16U,
        .phase_num={1U,1U,1U},.phase_den={16U,20U,25U},
        .vdda_uv=3300000U,.dac_gain_ppm=1000000U,
        .bus_min_uv=1000U,.bus_max_uv=50000000U,.max_age_ticks=32U};
    uint16_t code[3]={99,99,99};
    assert(am13e_bemf_reference_convert(&c,2048U,code));
    assert(code[0]==775U && code[1]==620U && code[2]==495U);
    /* Independent ideal voltage oracle with distinct ADC/DAC references. */
    for(unsigned raw=32U;raw<4095U;++raw){
        assert(am13e_bemf_reference_convert(&c,(uint16_t)raw,code));
        for(unsigned j=0;j<3U;++j){
            const unsigned pin=(unsigned)((double)raw/4096.0*2500000.0+0.5);
            const unsigned phase=(unsigned)((double)pin*16.0/2.0/c.phase_den[j]+0.5);
            const double ideal=(double)phase/3300000.0*4096.0-1.0;
            assert(code[j]==(unsigned)(ideal+0.5));
        }
    }
    assert(!am13e_bemf_reference_convert(&c,0U,code));
    assert(!am13e_bemf_reference_convert(&c,4095U,code));
    assert(!am13e_bemf_reference_convert(&c,4096U,code));
    c.adc_offset_code=10;c.adc_gain_ppm=2000000U;
    assert(am13e_bemf_reference_convert(&c,2058U,code));
    assert(code[0]==387U && code[1]==309U && code[2]==247U);
    c.phase_offset_uv[0]=10000;c.comparator_correction_uv[0]=-10000;
    assert(am13e_bemf_reference_convert(&c,2058U,code) && code[0]==387U);
    c.dac_offset_uv=100000;
    assert(am13e_bemf_reference_convert(&c,2058U,code));
    assert(code[0]==263U && code[1]==185U && code[2]==123U);
    c.phase_num[0]=1000U;assert(!am13e_bemf_reference_convert(&c,2048U,code));
    c.phase_num[0]=0U;assert(!am13e_bemf_reference_convert(&c,2048U,code));
    c.phase_num[0]=1U;c.adc_span_uv=UINT32_MAX;c.adc_offset_code=INT32_MIN;
    assert(!am13e_bemf_reference_convert(&c,2048U,code));
    AM13E_BemfReference ref={.acquisition_tick=UINT32_MAX-10U,.max_age_ticks=32U};
    assert(am13e_bemf_reference_fresh(&ref,5U));
    assert(!am13e_bemf_reference_fresh(&ref,23U));
    ref.max_age_ticks=0;assert(!am13e_bemf_reference_fresh(&ref,0U));
    puts("PASS SC5 unequal dividers/references, gain/offset, rounding, endpoints, overflow and age wrap");
}
int main(void)
{
    sc5_conversion();
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
