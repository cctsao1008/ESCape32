/* Asynchronous driver nFAULT trip; not independent current protection. */
#pragma once
#ifndef AM13E
#error "E62 AM13E-only hardware nFAULT Trip"
#endif
void am13e_app_motor_nfault_trip_init(void);
int am13e_app_motor_nfault_trip_ready(void);
