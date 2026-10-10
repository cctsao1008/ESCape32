/* Rel17 sine LUT and AM13E MCPWM compare scaling regression.
 * Verifies duty fraction equivalence, not physical gate waveforms.
 */
#include <assert.h>
#include <stdint.h>
extern const uint16_t sinedata[];
int main(void)
{
    enum { LEGACY_PWM_HZ=168000000, AM13E_PWM_HZ=100000000, CARRIER_HZ=24000 };
    const uint32_t old_ticks=LEGACY_PWM_HZ/CARRIER_HZ;
    const uint32_t new_ticks=AM13E_PWM_HZ/CARRIER_HZ;
    assert(old_ticks==7000U && new_ticks==4166U);
    for(unsigned phase=0;phase<360U;++phase) {
        assert(sinedata[phase]<=1120U);
        for(uint32_t power=0U;power<=120U;++power) {
            const uint32_t raw=((uint32_t)sinedata[phase]*power)>>7U;
            const uint32_t converted=(uint32_t)(((uint64_t)raw*new_ticks+
                                  old_ticks/2U)/old_ticks);
            assert(converted<new_ticks);
            const int64_t difference=(int64_t)converted*old_ticks-
                                     (int64_t)raw*new_ticks;
            assert(difference >= -(int64_t)(old_ticks/2U));
            assert(difference <=  (int64_t)(old_ticks/2U));
        }
    }
    /* Existing unscaled port made a 1050-count pulse on 4166 ticks;
     * correct Rel17 duty-preserving port produces 625 counts.
     */
    assert(((UINT64_C(1050)*new_ticks+old_ticks/2U)/old_ticks)==625U);
    return 0;
}
