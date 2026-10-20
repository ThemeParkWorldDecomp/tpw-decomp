// Basic portable types for the Theme Park World decompile.
#pragma once
#include <cstdint>

// Mac OS / Bullfrog 16.16 fixed point ("Fixed" in Mac Toolbox terms, "long" in the
// game source). 0x10000 == 1.0.
using Fixed = int32_t;
