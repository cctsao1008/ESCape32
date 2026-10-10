#include "motor_pad_route_plan.h"
#include <stddef.h>

int am13e_motor_pad_route_plan_valid(
    const uint32_t pins[AM13E_MOTOR_PAD_COUNT],
    const uint32_t pincm[AM13E_MOTOR_PAD_COUNT],
    uint32_t gpio_mask)
{
    if (pins == NULL || pincm == NULL || gpio_mask == 0U) return 0;
    uint32_t combined = 0U;
    for (unsigned i=0U;i<AM13E_MOTOR_PAD_COUNT;++i) {
        if (pins[i] == 0U || (pins[i] & (pins[i]-1U)) != 0U ||
            (combined & pins[i]) != 0U || pincm[i] >= 107U)
            return 0;
        for (unsigned j=0U;j<i;++j)
            if (pincm[i] == pincm[j]) return 0;
        combined |= pins[i];
    }
    return combined == gpio_mask;
}
