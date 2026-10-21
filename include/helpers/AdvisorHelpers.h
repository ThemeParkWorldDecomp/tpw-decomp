// AdvisorHelpers.h: file-local helpers that src/game/advisor/Advisor.cpp shares with BumperRide.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "platform/samsutils.h"

namespace v51116_swap {
static inline uint32_t Swap(uint32_t v) { return __builtin_bswap32(v); }

template<class T> static inline T* SwapPtr(T* p) { return reinterpret_cast<T*>(static_cast<uintptr_t>(Swap(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))))); }

}  // namespace v51116_swap
