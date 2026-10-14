#pragma once
#include <stdint.h>
typedef struct {
    uint32_t powered, polynomial, remainder, bytes, initializations;
} AM13E_TestCRCP;
extern AM13E_TestCRCP am13e_test_crcp;
#define CRC (&am13e_test_crcp)
enum { DL_CRCP_POLYNOMIAL_SIZE_32=32, DL_CRCP_BIT_REVERSED=1,
       DL_CRCP_INPUT_ENDIANESS_LITTLE_ENDIAN=2,
       DL_CRCP_OUTPUT_BYTESWAP_DISABLED=3 };
void DL_CRCP_enablePower(AM13E_TestCRCP *crcp);
void DL_CRCP_init(AM13E_TestCRCP *crcp,unsigned size,unsigned bit_order,
                   unsigned input_endian,unsigned output_swap);
void DL_CRCP_setPolynomial(AM13E_TestCRCP *crcp,uint32_t polynomial);
void DL_CRCP_setSeed32(AM13E_TestCRCP *crcp,uint32_t seed);
void DL_CRCP_feedData8(AM13E_TestCRCP *crcp,uint8_t byte);
uint32_t DL_CRCP_getResult32(const AM13E_TestCRCP *crcp);
