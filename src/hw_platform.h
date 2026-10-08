/* Platform header selection. Keep common ESCape32 data and control APIs independent of vendor SDKs. */
#pragma once
#ifdef ESCAPE32_AM13E
#include "hw_platform_am13e.h"
#else
#include "hw_platform_libopencm3.h"
#endif
