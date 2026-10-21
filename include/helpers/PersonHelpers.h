// PersonHelpers.h: file-local helpers that src/game/people/Person.cpp shares with Peep.cpp and PersonQueue.cpp. Before
// the regroup, the functions using them shared a source file (3 files in all), which had one copy of each; that copy
// now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "ThingReceivers.h"

static inline uint8_t* Raw(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }

template <class T>
static inline T& Fld(uint8_t* base, uint32_t off) { return *reinterpret_cast<T*>(base + off); }

// A one word MapPos pair from a map cell id: x = low 7 bits of id - 1 and an offset, y = the high bits and an offset.
static inline void CellPair(uint16_t id, uint8_t offX, uint8_t offY, MapPos& x, MapPos& y) {
    uint32_t v = static_cast<uint32_t>(id) - 1u;
    x.mCell = static_cast<uint8_t>(v & 0x7f);
    x.mOffset = offX;
    y.mCell = static_cast<uint8_t>((v >> 7) & 0xff);
    y.mOffset = offY;
}

// Fld_2: renamed from Fld, which is defined differently above (the files that used this one now call it so).
template <class T> static inline T& Fld_2(uint8_t* base, uint32_t offset) { return *reinterpret_cast<T*>(base + offset); }
