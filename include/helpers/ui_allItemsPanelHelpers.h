// ui_allItemsPanelHelpers.h: file-local helpers that src/game/ui/ui_allItemsPanel.cpp shares with ui_allKidsPanel.cpp.
// Before the regroup, the functions using them shared a source file (2 files in all), which had one copy of each; that
// copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "ui_controls.h"

static inline int32_t DataOf(const void* pointer) { return pointer != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(pointer)) : 0; }

static inline LCWord Number(int32_t value) {
    LCWord word;
    word.mRaw = 0;
    word.mInt = value;
    return word;
}

// The listing's divide by 100 (mulhw by 0x51eb851f, srawi 5, plus the sign): the plain truncating division.
static inline int32_t Div100(int32_t value) { return value / 100; }

// A byte (0 to 255) as the list's per-mille style cell: (byte << 10) / 100.
static inline int32_t PercentOfByte(uint32_t value) { return Div100(static_cast<int32_t>((value & 0xffu) << 10)); }
