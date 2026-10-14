#pragma once
#include "../../app/aux_mock/dl_gpio.h"
#define DL_GPIO_INVERSION_ENABLE 1U
#define DL_GPIO_RESISTOR_NONE 0U
extern unsigned test_pin_invert[96];
static inline bool DL_GPIO_isPeripheralConnected(unsigned pin)
{ return test_function[pin]!=0U; }
static inline void DL_GPIO_initPeripheralInputFunctionFeatures(
    unsigned pin,unsigned function,unsigned invert,unsigned resistor,unsigned wake)
{ (void)resistor;(void)wake;test_function[pin]=function;test_input[pin]=1U;test_pin_invert[pin]=invert; }
static inline void DL_GPIO_initPeripheralOutputFunction(unsigned pin,unsigned function)
{ test_function[pin]=function; }

extern unsigned test_invert;
static inline void DL_GPIO_setDataInversion(unsigned pin,unsigned invert)
{ test_pin_invert[pin]=invert;test_invert=invert; }
