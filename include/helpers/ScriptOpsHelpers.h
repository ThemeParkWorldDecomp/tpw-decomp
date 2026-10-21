// ScriptOpsHelpers.h: file-local helpers that src/game/ScriptOps.cpp shares with Script.cpp. Before the regroup, the
// functions using them shared a source file (2 files in all), which had one copy of each; that copy now lives here,
// static inline, so each file that includes this header keeps its own internal copy. The helpers it has in common with
// ScriptHelpers.h come from that header, which a file of both groups includes too.
#pragma once

#include "ScriptHelpers.h"   // the shared helpers

// The descriptor words the finders look for (kScriptCommandAddress + 8 * index: IF 1, ENDIF 2, ELSE 3).
constexpr uint32_t kIfWord = 0x10266960;

constexpr uint32_t kEndifWord = 0x10266968;

// The code address of the word at mPos, counted from `base` (slwi 2; add), and the code end test (cmpw, signed).
static inline uint32_t AddressAt(uint32_t base) { return base + static_cast<uint32_t>(gScriptCurrent->mPos) * 4; }

static inline bool BeforeEnd(uint32_t address) { return static_cast<int32_t>(address) < static_cast<int32_t>(kScriptCodeEnd); }

// The compiler inlined two levels of SCR_FindNextEndif's own recursion into itself (432 bytes), and the three copies differ:
//   level 0 (the function): no tail at all;
//   level 1: leaves with --IF depth (done by the caller here);
//   level 2: tests "address >= end" and reports "End of script before ENDIF found", then --IF depth; its own nested IF is the real call.
// Every level steps over a nested IF with ++mPos, ++depth, the inner walk, --depth, ++mPos.
static inline void WalkToEndif(int level) {
    const uint32_t base = gScriptCurrent->mCodeBase;
    for (;;) {
        const uint32_t address = AddressAt(base);
        if (!BeforeEnd(address)) break;
        const uint32_t word = SCR_CodeWord(address);
        if (word == kEndifWord) break;
        if (word == kIfWord) {
            gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
            gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth + 1;
            if (level < 2) {
                WalkToEndif(level + 1);
            } else {
                SCR_FindNextEndif();
            }
            gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth - 1;
        }
        gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
    }
    if (level == 2) {
        if (!BeforeEnd(AddressAt(base))) SCR_Error("End of script before ENDIF found");
    }
}
