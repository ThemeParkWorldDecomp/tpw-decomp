// MslSupportHelpers.h: file-local helpers that src/game/MslSupport.cpp shares with FTPClient.cpp, UIInterface.cpp and
// ui_2dmap.cpp. Before the regroup, the functions using them shared a source file (3 files in all), which had one copy
// of each; that copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "ui_main.h"

template <class T>
static inline T& At(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

static inline uint32_t ToWord(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

// A Mac word that points into the arena is an arena offset here (0 is null).
static inline uint32_t* Words(uint32_t offset) { return offset != 0 ? reinterpret_cast<uint32_t*>(tpw::gArenaBase + offset) : nullptr; }

namespace v51126 {
// Mac words are arena offsets on the host.
static inline void* P(uint32_t address) { return address != 0 ? static_cast<void*>(tpw::gArenaBase + address) : nullptr; }

static inline uint32_t* W(uint32_t address) { return static_cast<uint32_t*>(P(address)); }

static inline uint32_t A(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }

}  // namespace v51126
