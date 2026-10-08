/* Host regression against rel17's original literal six-step encoding. */
#include <assert.h>
#include <stdint.h>
#include "hw_six_step.h"

int main(void)
{
    static const uint16_t original[6] = {
        0x175U, 0xd9U, 0x1abU, 0x72U, 0x1deU, 0xacU
    };
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        for (unsigned step = 1; step <= 6; ++step) {
            unsigned x = original[step - 1U];
            unsigned m = x >> 3;
            hw_six_step_t actual = hw_six_step_decode(step, reverse);
            assert(actual.positive == (x & m));
            assert(actual.negative == ((~x) & m));
            assert(actual.comparator == ((m >> 3) ^ (reverse << 2)));
            assert((actual.positive & actual.negative) == 0U);
        }
    }
    return 0;
}
