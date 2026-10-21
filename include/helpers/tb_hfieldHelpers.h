// tb_hfieldHelpers.h: file-local helpers that src/engine/tb_hfield.cpp shares with DrawPrimitive.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <utility>

// The middle of a range as the sorts take it: the 32-bit sum shifted right arithmetically.
static inline int32_t Middle(int32_t low, int32_t high) {
    return static_cast<int32_t>(static_cast<uint32_t>(low) + static_cast<uint32_t>(high)) >> 1;
}
