// Test library for SetNavRoute and BumpToPoint (unit v5656; libtpw_unit_SetNavRoute.dylib, tests/test_SetNavRoute.py), main app:
//   src/game/rides/BumperRide.cpp   both functions and the two thrust limit globals.
//
// The callees outside the unit are recording fakes; the collision objects they answer are set by tpw_snr_objs.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"

using namespace calllog;

namespace {
tabCOLOBJECT* gObjs[3];
}

// the tag of an object is its x word (unique per case)
static int64_t Tag(tabCOLOBJECT* p) { return p ? p->x : 0; }

tabCOLOBJECT* PointInAnyColObject(int32_t x, int32_t y) {
    ev("PointInAnyColObject", {x, y});
    return gObjs[ret("PointInAnyColObject") & 3];
}

int32_t DoAreasIntersect(tabCOLOBJECT* a, tabCOLOBJECT* b) {
    ev("DoAreasIntersect", {Tag(a), Tag(b)});
    return static_cast<int32_t>(ret("DoAreasIntersect"));
}

tabCOLOBJECT* ClosestIntersectingColObjectToPoint(tabCOLOBJECT* area, int32_t px, int32_t py, int32_t includeSelf) {
    ev("Closest", {Tag(area), px, py, includeSelf});
    return gObjs[ret("Closest") & 3];
}

NS_MacDoze::tagPOINT* NavPointInArea(tabCOLOBJECT* object, NS_MacDoze::tagPOINT* out) {
    ev("NavPointInArea", {Tag(object)});
    int32_t* p = reinterpret_cast<int32_t*>(out);
    p[0] = static_cast<int32_t>(ret("NavX"));
    p[1] = static_cast<int32_t>(ret("NavY"));
    return out;
}

void TRACE(const char*, ...) { ev("TRACE", {}); }

int32_t BMP_Distance(int32_t dx, int32_t dy) {
    ev("BMP_Distance", {dx, dy});
    return static_cast<int32_t>(ret("BMP_Distance"));
}

void BMP_Thrust(tagBUMP* bump, int32_t amount) {
    ev("BMP_Thrust", {amount, bump->flags});
}

extern "C" {

uint64_t tpw_snr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"limitA", &gBumpThrustLimitA},
        {"limitB", &gBumpThrustLimitB},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_snr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_snr_off(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

void tpw_snr_objs(uint64_t a, uint64_t b) {
    gObjs[0] = nullptr;
    gObjs[1] = reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(a));
    gObjs[2] = reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(b));
    gObjs[3] = nullptr;
}

int32_t tpw_snr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, col));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, owner));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLOBJECT, x));
    return n;
}

int32_t tpw_snr_set(uint64_t bump, int32_t x, int32_t y) {
    return SetNavRoute(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)), x, y);
}

int32_t tpw_snr_bump(uint64_t bump, int32_t x, int32_t y) {
    return BumpToPoint(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)), x, y);
}

}  // extern "C"
