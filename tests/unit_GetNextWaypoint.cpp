// Test library for GetNextWaypoint (unit v5198; libtpw_unit_GetNextWaypoint.dylib, tests/test_GetNextWaypoint.py), main app:
//   src/GetNextWaypoint.cpp   GetNextWaypoint
//
// The callees outside the unit (CWorld::GetRand, HeightAtPos_NAV, GetWorldGSquareHeight) are recording fakes answering from the test's
// queues; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "World.h"

using namespace calllog;

int32_t gFlyZoneA;
int32_t gFlyZoneB;
int32_t gFlyZoneC;
int32_t gFlyZoneD;
int32_t gFlyMinHeight;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
CWorld* gGameWorld;

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

int32_t HeightAtPos_NAV(int32_t x, int32_t z) {
    ev("HeightAtPos_NAV", {x, z});
    return static_cast<int32_t>(ret("HeightAtPos_NAV"));
}

float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("GetWorldGSquareHeight", {x, y, lab(extra)});
    const uint32_t bits = static_cast<uint32_t>(ret("GetWorldGSquareHeight"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

extern "C" {

uint64_t tpw_gnw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gFlyZoneA", &gFlyZoneA},   {"gFlyZoneB", &gFlyZoneB},   {"gFlyZoneC", &gFlyZoneC},
        {"gFlyZoneD", &gFlyZoneD},   {"gFlyMinHeight", &gFlyMinHeight},
        {"gHeightfieldWidth", &gHeightfieldWidth}, {"gHeightfieldHeight", &gHeightfieldHeight},
        {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gnw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gnw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, mX4));
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, m8));
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, mZC));
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, mX4c));
    out[n++] = static_cast<int64_t>(offsetof(FLYBOY, mPrevZ60));
    return n;
}

void tpw_gnw_run(uint64_t fly) { GetNextWaypoint(reinterpret_cast<FLYBOY*>(static_cast<uintptr_t>(fly))); }

}  // extern "C"
