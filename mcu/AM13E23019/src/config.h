/*
 * E62 rel17 compile-probe target configuration.
 * This header deliberately contains no pin/peripheral register aliases.
 * Numerical clock, dead time, and comparator mappings are not frozen by
 * this probe; they must come from validated AM13 hardware configuration.
 */
#pragma once

#ifndef ESCAPE32_AM13E
#error "AM13E target config selected without ESCAPE32_AM13E"
#endif
#ifndef CLK
#error "Define validated AM13E control clock frequency (CLK) before compiling rel17"
#endif
#ifndef DEAD_TIME
#error "Define validated AM13E dead-time conversion input (DEAD_TIME)"
#endif
#ifndef COMP_MAP
#error "Define board-validated comparator phase ordering (COMP_MAP)"
#endif
