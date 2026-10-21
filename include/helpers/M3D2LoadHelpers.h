// M3D2LoadHelpers.h: file-local helpers that src/engine/M3D2Load.cpp shares with TPHField.cpp. Before the regroup, the
// functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "platform/samsutils.h"

static inline void SwapWords32(void* words, int32_t count) {
    SamsUtilities::_USwapBlock32(static_cast<uint32_t*>(words), count);
}
