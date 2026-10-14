/* CRCP DriverLib boundary model, NOT TI silicon verification.
 * Use MSB-first polynomial arithmetic plus explicit bit reversal, separate
 * from the wire oracle's reflected 0xedb88320 implementation. */
#include "dl_crcp.h"
#include <assert.h>
AM13E_TestCRCP am13e_test_crcp;
static uint32_t reverse(uint32_t v,unsigned bits)
{
    uint32_t out=0U;
    while(bits--){out=(out<<1U)|(v&1U);v>>=1U;}
    return out;
}
void DL_CRCP_enablePower(AM13E_TestCRCP *c) { c->powered=1U; }
void DL_CRCP_init(AM13E_TestCRCP *c,unsigned size,unsigned bit,
                   unsigned endian,unsigned swap)
{
    assert(c->powered && size==DL_CRCP_POLYNOMIAL_SIZE_32 &&
           bit==DL_CRCP_BIT_REVERSED &&
           endian==DL_CRCP_INPUT_ENDIANESS_LITTLE_ENDIAN &&
           swap==DL_CRCP_OUTPUT_BYTESWAP_DISABLED);
    ++c->initializations;
    c->bytes=0U;
}
void DL_CRCP_setPolynomial(AM13E_TestCRCP *c,uint32_t polynomial)
{ assert(polynomial==UINT32_C(0x04c11db7)); c->polynomial=polynomial; }
void DL_CRCP_setSeed32(AM13E_TestCRCP *c,uint32_t seed)
{ assert(seed==UINT32_MAX); c->remainder=seed; }
void DL_CRCP_feedData8(AM13E_TestCRCP *c,uint8_t byte)
{
    c->remainder ^= reverse(byte,8U)<<24U;
    for(unsigned n=0U;n<8U;++n)
        c->remainder=(c->remainder<<1U)^
            ((c->remainder&UINT32_C(0x80000000))?c->polynomial:0U);
    ++c->bytes;
}
uint32_t DL_CRCP_getResult32(const AM13E_TestCRCP *c)
{ return reverse(c->remainder,32U); }
