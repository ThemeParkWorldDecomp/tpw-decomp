// InterfaceTimerHelpers.h: file-local helpers that src/game/ui/InterfaceTimer.cpp shares with AdvisorScoring.cpp.
// Before the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now
// lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "UI_Base.h"

// The table check of the destructor (the same test as UI_RemoveTimerResult's): the timer is non-NULL, the table exists, and the timer
// is at or after the table's start and before its end (unsigned compares, the host's stride is sizeof(InterfaceTimer)).
static inline bool InTimerTable(const InterfaceTimer* timer) {
    if (timer == nullptr || gUITimers == nullptr) return false;
    const uintptr_t at = reinterpret_cast<uintptr_t>(timer);
    const uintptr_t first = reinterpret_cast<uintptr_t>(gUITimers);
    return at >= first && at < first + static_cast<intptr_t>(gUITimerCount) * static_cast<intptr_t>(sizeof(InterfaceTimer));
}
