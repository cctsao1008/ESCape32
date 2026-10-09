/* E62 AM13E23019 PB15/GPIO47 nFAULT input-only service. */
#pragma once
#if !defined(AM13E)
#error "AM13E GPIO runtime service is not for legacy MCUs"
#endif

/* Returns nonzero for an asserted or unavailable nFAULT input.
 * Must not be used as proof of hardware fault protection until board tested.
 */
void initgpio(void);
int am13e_app_nfault_asserted(void);
