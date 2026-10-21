// CoasterSoundsHelpers.h: file-local helpers that src/game/rides/CoasterSounds.cpp shares with CoasterTrain.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "ppc_math.h"

// ModF (0x1004a75c) as the original expands it in place: the fraction of the value widened to double, as a float, and the whole part
// truncated to an int (fctiwz).
static inline float SplitFraction(float value, int32_t& whole) {
    double integral;
    const double fraction = std::modf(static_cast<double>(value), &integral);
    whole = ppc_fctiwz(integral);
    return static_cast<float>(fraction);
}
