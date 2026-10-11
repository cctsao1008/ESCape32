/* BEMF capture clock conversion: preserve fractional-MHz ECAP rates.
 * Host-only arithmetic regression, NOT a silicon timing measurement.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t to_us(uint32_t ticks,uint32_t ticks_per_ms) {
    assert(ticks_per_ms>0);
    return (uint32_t)(((uint64_t)ticks*1000U+ticks_per_ms/2U)/ticks_per_ms);
}
static uint32_t timeout_ticks(uint32_t ms_ticks) {
    return (uint32_t)(((uint64_t)32768U*ms_ticks+999U)/1000U);
}
int main(void){
 const uint32_t clocks[]={24000000U,25000000U,62500000U,96000000U,100000000U,125000000U,133333333U,200000000U};
 for(unsigned i=0;i<sizeof(clocks)/sizeof(clocks[0]);++i){
  const uint32_t msec=(clocks[i]+500U)/1000U;
  assert(msec>0&&msec<=200000U);
  for(unsigned ms=1;ms<=30;ms++){
   uint32_t ticks=(uint32_t)(((uint64_t)clocks[i]*ms+500U)/1000U);
   uint32_t converted=to_us(ticks,msec);
   int difference=(int)converted-(int)(ms*1000U);
   assert(difference>=-1 && difference<=1);
  }
  uint32_t t=timeout_ticks(msec);
  assert(to_us(t,msec)>=32768U);
  assert(to_us(t,msec)<=32769U);
 }
 puts("BEMF ECAP rational clock/timeout: 8 clock rates PASS");
}
