#pragma once
#ifdef HALL_MAP
int hallcode(void);
void am13e_app_motor_hall_gpio_init(void);
int am13e_app_motor_hall_ready(void);
void GPIO0_IRQHandler(void);
void EQEP0_IRQHandler(void);
#endif
