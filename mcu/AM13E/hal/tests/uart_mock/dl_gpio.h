#pragma once
#include "../../../../../src/tests/AM13E/aux_mock/dl_gpio.h"
#define DL_GPIO_INVERSION_ENABLE 1U
#define DL_GPIO_RESISTOR_NONE 0U
static inline bool DL_GPIO_isPeripheralConnected(unsigned pin)
{ return test_function[pin]!=0U; }
static inline void DL_GPIO_initPeripheralInputFunctionFeatures(
    unsigned pin,unsigned function,unsigned invert,unsigned resistor,unsigned wake)
{ (void)invert;(void)resistor;(void)wake;test_function[pin]=function;test_input[pin]=1U; }
static inline void DL_GPIO_initPeripheralOutputFunction(unsigned pin,unsigned function)
{ test_function[pin]=function; }
