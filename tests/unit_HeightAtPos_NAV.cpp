// Test library for HeightAtPos_NAV and GetNextHoldingWaypoint (unit v5216; libtpw_unit_HeightAtPos_NAV.dylib, tests/test_HeightAtPos_NAV.py), main app:
//   src/GetNextWaypoint_HeightAtPos_NAV.cpp
//
// GetWorldGSquareHeight is a recording fake answering from the test's queue. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"

using namespace calllog;

int32_t gFlyZoneB;
int32_t gFlyZoneA;
int32_t gFlyZoneD;
int32_t gFlyZoneC;
int32_t gHeightfieldHeight;
int32_t gHeightfieldWidth;
void* gFlyBases[100];

float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("GetWorldGSquareHeight", {x, y, lab(extra)});
    const uint32_t bits = static_cast<uint32_t>(ret("GetWorldGSquareHeight"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

extern "C" {

uint64_t tpw_hn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"zoneD", &gFlyZoneD}, {"zoneB", &gFlyZoneB}, {"zoneA", &gFlyZoneA}, {"zoneC", &gFlyZoneC},
        {"hfH", &gHeightfieldHeight}, {"hfW", &gHeightfieldWidth}, {"bases", gFlyBases}, {"holdMax", &gFlyHoldMax},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, mX4c));
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, mPrevZ60));
    return n;
}

int32_t tpw_hn_height(int32_t x, int32_t z) { return HeightAtPos_NAV(x, z); }

void tpw_hn_hold(uint64_t fly) { GetNextHoldingWaypoint(reinterpret_cast<FLYBOY*>(static_cast<uintptr_t>(fly))); }

}  // extern "C"
