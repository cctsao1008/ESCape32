/* Asynchronous driver nFAULT trip; not independent current protection. */
#pragma once
#ifndef AM13E
#error "AM13E reference AM13E-only hardware nFAULT Trip"
#endif
void am13e_app_motor_nfault_trip_init(void);
void am13e_app_motor_oc_trip_init(void);
int am13e_app_motor_oc_trip_ready(void);
int am13e_app_motor_nfault_trip_ready(void);
