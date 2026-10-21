// ui_2dmapHelpers.h: file-local helpers that src/game/ui/ui_2dmap.cpp shares with UIGrapher.cpp. Before the regroup,
// the functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>

static inline uint32_t A(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
