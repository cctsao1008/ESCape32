#include "servo_calibration.h"
#include <stddef.h>

int am13e_servo_calibration_step(AM13E_ServoCalibration *s,
                                  unsigned p, int enabled)
{
    if (s == NULL || s->finished) return AM13E_SERVO_CAL_DONE;
    /* Original strict bounds, including both 11/22ms source periods. */
    const int eligible = (p < 22880U && p > 21120U) ||
        (p < 20800U && p > 19200U) || (p < 11440U && p > 10560U) ||
        (p < 10400U && p > 9600U) || (p < 8320U && p > 7680U) ||
        (p < 5200U && p > 4800U) || (p < 4160U && p > 3840U) ||
        (p < 3120U && p > 2880U) || (p < 2080U && p > 1920U);
    if (enabled && eligible) {
        s->error += (int)p - (int)((p + 500U) / 1000U) * 1000;
        if (++s->count & 3U) return AM13E_SERVO_CAL_WAIT;
        if (s->error > s->high) {
            s->low = -s->error;
            s->error = 0;
            return AM13E_SERVO_CAL_SLOW;
        }
        if (s->error < s->low) {
            s->high = -s->error;
            s->error = 0;
            return AM13E_SERVO_CAL_FAST;
        }
    }
    s->finished = 1U;
    return AM13E_SERVO_CAL_DONE;
}
