// tb_errorHelpers.h: file-local helpers that src/engine/tb_error.cpp shares with DrawPrimitive.cpp. Before the regroup,
// the functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "tb_resource_display.h"

// `pointer` moved by a byte count the Mac computes in 32 bits (a negative one goes back).
static inline uintptr_t Moved(const void* pointer, uint32_t bytes) {
    return reinterpret_cast<uintptr_t>(pointer) + static_cast<intptr_t>(static_cast<int32_t>(bytes));
}
