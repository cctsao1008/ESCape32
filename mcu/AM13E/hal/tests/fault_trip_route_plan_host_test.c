/* Numerical MCU route regression, not physical electrical qualification. */
#include "fault_trip_route_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    unsigned tested = 0U;
    for (unsigned input=0U;input<16U;++input) {
        const unsigned source = input<12U ? 0x14U+input :
                                              0x100U+input-12U;
        for (unsigned trip=0U;trip<8U;++trip) {
            const uint32_t ost = UINT32_C(0x10000)<<trip;
            assert(am13e_fault_trip_route_fields_valid(
                47U,input,trip,source,ost,ost));
            assert(!am13e_fault_trip_route_fields_valid(
                47U,input,trip,source^1U,ost,ost));
            assert(!am13e_fault_trip_route_fields_valid(
                47U,input,trip,source,ost<<1U,ost));
            assert(!am13e_fault_trip_route_fields_valid(
                47U,input,trip,source,ost,ost<<1U));
            ++tested;
        }
    }
    assert(!am13e_fault_trip_route_fields_valid(
        107U,1U,0U,0x15U,0x10000U,0x10000U));
    assert(!am13e_fault_trip_route_fields_valid(
        47U,16U,0U,0x100U,0x10000U,0x10000U));
    assert(!am13e_fault_trip_route_fields_valid(
        47U,1U,8U,0x15U,0x10000U,0x10000U));
    printf("PASS: %u AM13E route pairs, mismatches and bounds\n",tested);
    return 0;
}
