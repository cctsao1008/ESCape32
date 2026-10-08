/* Compile-only guard for the rel17 shared data model without TI or STM32 headers. */
#include <stddef.h>
#include <stdint.h>
#include "esc_types.h"

_Static_assert(sizeof(((Cfg *)0)->id) == sizeof(uint16_t), "Cfg.id width");
_Static_assert(sizeof(((Cfg *)0)->music) == 256U, "Cfg.music width");
_Static_assert(offsetof(Cfg, id) == 0U, "Cfg.id offset");
_Static_assert(offsetof(Cfg, revision) == sizeof(uint16_t), "Cfg.revision offset");
_Static_assert(offsetof(Cfg, music) > offsetof(Cfg, prot_park), "Cfg field order");
_Static_assert(sizeof(PID) == 6U * sizeof(int), "PID layout");

int main(void)
{
    Cfg cfg = {0};
    PID pid = {0};
    return (cfg.id != 0U || pid.Kp != 0);
}
