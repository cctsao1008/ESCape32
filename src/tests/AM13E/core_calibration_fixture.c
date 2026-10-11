/* State-only adapter for existing calibration vectors. The implementation
 * exercised is the original production calibirq(), including its actual math. */
#define REL17_CALIBRATION_TEST_ADAPTER
void am13e_app_servo_clock_trim(int direction);
#include "../../io.c"
#include "interfaces/servo_calibration.h"
static int trim_action;
void am13e_app_servo_clock_trim(int direction) {
    trim_action=direction<0 ? AM13E_SERVO_CAL_SLOW : AM13E_SERVO_CAL_FAST;
}
int am13e_servo_calibration_step(AM13E_ServoCalibration *s,unsigned period,int enabled) {
    if(!s || s->finished)return AM13E_SERVO_CAL_DONE;
    servo_calibration_state.count=s->count;servo_calibration_state.error=s->error;
    servo_calibration_state.high=s->high;servo_calibration_state.low=s->low;
    servo_capture_period=period;servo_calibration_finished=0;cfg.throt_cal=(char)enabled;
    trim_action=AM13E_SERVO_CAL_WAIT;
    calibirq();
    s->count=servo_calibration_state.count;s->error=servo_calibration_state.error;
    s->high=servo_calibration_state.high;s->low=servo_calibration_state.low;
    s->finished=(uint8_t)servo_calibration_finished;
    return s->finished ? AM13E_SERVO_CAL_DONE : trim_action;
}
