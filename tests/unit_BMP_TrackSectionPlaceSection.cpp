// Test library for BMP_TrackSectionPlaceSection (unit v5566; libtpw_unit_BMP_TrackSectionPlaceSection.dylib,
// tests/test_BMP_TrackSectionPlaceSection.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_TrackSectionPlaceSection
//
// TRACE is a fake recording its text. The list at 0x102b56c0 (gColList) is defined here, big enough for the words the function reads.
// The header is not included here (its gColList is a smaller struct); the function is declared below.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint8_t gColList[0x100];

int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9);

// ---- the fakes
void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

extern "C" {

uint64_t tpw_tsp_addr(const char* name) {
    if (std::strcmp(name, "list") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gColList));
    return 0;
}

uint64_t tpw_tsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_tsp_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_tsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gColList));
    return n;
}

uint32_t tpw_tsp_a(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, int32_t f8, uint32_t f9) {
    return static_cast<uint32_t>(BMP_TrackSectionPlaceSection(id, x, y, type, f7, static_cast<uint32_t>(f8), static_cast<int32_t>(f9)));
}

}  // extern "C"
