#include <assert.h>
#include <stdint.h>
#include "esc_math.h"

int main(void)
{
    const char sample[] = "123456789";
    assert(crc8(sample, 9) == 0xf4);
    /* High-bit bytes must never create negative lookup-table indices. */
    const char bytes[] = {(char)0x80, (char)0xff, (char)0x00, (char)0xa5};
    (void)crc8(bytes, 4);
    (void)crc8dvbs2(bytes, 4);
    (void)crc16ccitt(bytes, 4);
    (void)crc16xmodem(bytes, 4);
    assert(crc8dvbs2(sample, 9) == 0xbc);
    assert(crc16ccitt(sample, 9) == 0x2189);
    assert(crc16xmodem(sample, 9) == 0x31c3);
    assert(scale(5, 0, 10, 0, 100) == 50);
    assert(scale(-1, 0, 10, 0, 100) == 0);
    assert(scale(11, 0, 10, 0, 100) == 100);
    int s = -1;
    assert(smooth(&s, 100, 3) == 100);
    PID pid = {.Kp=2, .Ki=3, .Kd=4, .Li=10};
    initpid(&pid, 0);
    assert(calcpid(&pid, 5, 1) == 40);
    assert(pid.i == 4 && pid.x == 5);
    return 0;
}
