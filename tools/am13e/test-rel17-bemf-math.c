/* Host equivalence regression against the original ESCape32 rel17 ISR math. */
#include <assert.h>
#include "hw_bemf_rel17_math.h"

static void check(int capture, int old_interval, int ertm, int timing)
{
    const int u = old_interval * 3;
    const int fast = (capture < (u >> 2) || capture > (u >> 1)) && ertm < 2000;
    const int interval = (capture + u) >> 2;
    const int raw_delay = (interval - (interval * timing >> 5)) >> 1;
    const int delay = raw_delay > 1 ? raw_delay : 1;
    const hw_bemf_rel17_result_t actual =
        hw_bemf_rel17_calculate(capture, old_interval, ertm, timing);

    assert(actual.fast == fast);
    assert(actual.interval == interval);
    assert(actual.delay == delay);
}

int main(void)
{
    static const int revolutions[] = {0, 1999, 2000, 100000000};
    static const int timing[] = {0, 1, 16, 31};
    for (int previous = 1; previous <= 10000; previous += 97) {
        for (int capture = 1; capture <= 20000; capture += 113) {
            for (unsigned r = 0; r < sizeof revolutions / sizeof revolutions[0]; ++r) {
                for (unsigned t = 0; t < sizeof timing / sizeof timing[0]; ++t)
                    check(capture, previous, revolutions[r], timing[t]);
            }
        }
    }
    return 0;
}
