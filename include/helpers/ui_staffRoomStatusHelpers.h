// ui_staffRoomStatusHelpers.h: file-local helpers that src/game/ui/ui_staffRoomStatus.cpp shares with
// ui_showStatus.cpp. Before the regroup, the functions using them shared a source file (1 file), which had one copy of
// each; that copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>
#include <new>

static inline uint32_t* MakeTable(uint32_t*& table, uint32_t* (*make)()) {
    if (table == nullptr) table = make();
    return table;
}
