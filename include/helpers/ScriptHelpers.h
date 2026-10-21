// ScriptHelpers.h: file-local helpers that src/game/Script.cpp shares with ScriptOps.cpp. Before the regroup, the
// functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "analyser.h"
#include <cstring>

static inline int32_t FloatWord(float value) {
    int32_t word;
    std::memcpy(&word, &value, sizeof word);
    return word;
}

// The operand at the current script's mPos; mPos moves on by one (the PK_ functions' common first step).
static inline uint32_t NextOperand() {
    const int32_t pos = gScriptCurrent->mPos;
    gScriptCurrent->mPos = pos + 1;
    return SCR_CodeWord(gScriptCodeBase + static_cast<uint32_t>(pos) * 4);
}

// The next word's index for a code address (an arithmetic shift, no rounding toward zero).
static inline int32_t PositionOf(uint32_t address, uint32_t base) { return static_cast<int32_t>(address - base) >> 2; }
