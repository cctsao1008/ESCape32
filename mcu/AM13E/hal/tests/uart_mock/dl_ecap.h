#pragma once
#define ECAP0 ((void*)0)
#define ECAP0_INT_IRQn 3
static inline unsigned DL_ECAP_getTimeStampCounter(void *e){(void)e;return 0U;}
static inline void DL_ECAP_disableTimeStampCapture(void *e){(void)e;assert(0);}
