#include "analog_sampling_backend.h"
#include "analog_reference.h"
#include <soc.h>
#include <dl_common.h>
int am13e_mcu_adc_scan_initialize(ADC_Regs *adc,DL_ADC_CHANNEL soc0,
    DL_ADC_CHANNEL soc1,DL_ADC_CHANNEL soc2,int soc3,uint32_t mclk_hz)
{
    if(__get_PRIMASK()!=1U || (adc!=ADC0 && adc!=ADC1 && adc!=ADC2) ||
       soc0>31U || soc1>31U || soc2>31U || soc0==soc1 || soc0==soc2 ||
       soc1==soc2 || soc3>31 || soc3 < -1 ||
       (soc3>=0 && ((unsigned)soc3==soc0 || (unsigned)soc3==soc1 ||
                    (unsigned)soc3==soc2)) || mclk_hz<2000U)return 0;
    DL_ADC_reset(adc);DL_ADC_enablePower(adc);
    if(!DL_ADC_isPowerEnabled(adc))return 0;
    DL_ADC_Config config;DL_ADC_initParamsSetDefault(&config);
    config.coreConfig.clkPrescale=DL_ADC_CLOCK_DIVIDE_8_0;
    config.socConfig[0].channel=soc0;
    config.socConfig[1].channel=soc1;
    config.socConfig[2].channel=soc2;
    if(soc3>=0)config.socConfig[3].channel=(DL_ADC_CHANNEL)soc3;
    const DL_ADC_SOC_NUMBER last=soc3>=0?DL_ADC_SOC_NUMBER3:DL_ADC_SOC_NUMBER2;
    config.seqConfig.endSocNumber=last;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].enableSequencer=true;
    /* Device datasheet6.9.1 requires cap reset enabled to VREFLO. */
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].enableSampleCapReset=true;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].capResetVal=DL_ADC_SAMPLE_CAP_RESET_SELECT_VREFLO;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].sampleWindow=AM13E_ADC_ACQ_WINDOW_CYCLES;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].trigger=DL_ADC_TRIGGER_SOFTWARE;
    config.seqConfig.seqNConfig[AM13E_ADC_SEQUENCE].socStartNumber=DL_ADC_SOC_NUMBER0;
    config.intConfig.pulseMode=DL_ADC_PULSE_END_OF_CONV;
    config.intConfig.intNConfig[AM13E_ADC_IRQ].enableInterrupt=true;
    config.intConfig.intNConfig[AM13E_ADC_IRQ].trigger=last;
    DL_ADC_init(adc,&config);
    DL_Common_delayCycles(mclk_hz/2000U); /* Existing 500us startup convention. */
    DL_ADC_clearInterruptStatus(adc,AM13E_ADC_IRQ);
    return 1;
}
