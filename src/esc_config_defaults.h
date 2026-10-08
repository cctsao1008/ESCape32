/* ESCape32 target-independent config capability defaults. */
#pragma once
#ifndef SENS_MAP
#define SENS_MAP 0
#define SENS_CNT 0
#elif SENS_MAP <= 0xff
#define SENS_CNT 1
#elif SENS_MAP <= 0xffff
#define SENS_CNT 2
#else
#define SENS_CNT 3
#endif
#ifndef LED_MAP
#ifdef LED_WS2812
#define LED_CNT 3
#else
#define LED_CNT 0
#endif
#elif LED_MAP <= 0xff
#define LED_CNT 1
#elif LED_MAP <= 0xffff
#define LED_CNT 2
#elif LED_MAP <= 0xffffff
#define LED_CNT 3
#else
#define LED_CNT 4
#endif
#ifndef BEC_MIN
#define BEC_MIN 0
#endif
#ifndef BEC_MAX
#define BEC_MAX (BEC_MIN + 3)
#endif
