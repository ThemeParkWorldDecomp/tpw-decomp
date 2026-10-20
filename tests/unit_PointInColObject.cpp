// Test library for the collision object tests (unit v5367; libtpw_unit_PointInColObject.dylib, tests/test_PointInColObject.py), main app:
//   src/BMP_InsideArea_PointOutAllInverse_PointInColObject.cpp   PointInColObject, DoAreasIntersect, ClosestIntersectingColObjectToPoint
//
// PointOutAllInverse is a fake recording its arguments and answering from the queue "PointOut". gColList (main app data) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"

using namespace calllog;

tabCOLLIST gColList;

int32_t PointOutAllInverse(int32_t x, int32_t y) {
    ev("PointOut", {x, y});
    return static_cast<int32_t>(ret("PointOut"));
}

extern "C" {

uint64_t tpw_pic_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"colList", &gColList},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pic_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word a list field holds for an arena address.
uint32_t tpw_pic_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_pic_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabCOLOBJECT));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, first));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, count));
    return n;
}

int32_t tpw_pic_PointInColObject(int32_t x, int32_t y, uint64_t object) {
    return PointInColObject(x, y, reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(object)));
}

int32_t tpw_pic_DoAreasIntersect(uint64_t a, uint64_t b) {
    return DoAreasIntersect(reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(a)),
                            reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(b)));
}

uint64_t tpw_pic_Closest(uint64_t area, int32_t px, int32_t py, int32_t self) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        ClosestIntersectingColObjectToPoint(reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(area)), px, py, self)));
}

}  // extern "C"
