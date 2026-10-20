// Test library for the bump area functions (unit v5050; libtpw_unit_BMP_InsideArea.dylib, tests/test_BMP_InsideArea.py), main app:
//   src/BMP_InsideArea.cpp   BMP_InsideArea, BMP_OutsideArea
//
// PointInColObject is a fake that records (x, y, object label) and answers from the test's queue. atan2, sqrt and labs are the C library's
// on both sides. The sine table and the collision list are globals of other files, defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"

using namespace calllog;

int32_t gHostSineTable[512];
tabCOLLIST gColList;

int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object) {
    ev("PointInColObject", {x, y, lab(object)});
    return static_cast<int32_t>(ret("PointInColObject"));
}

extern "C" {

uint64_t tpw_bmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sine", gHostSineTable},
        {"list", &gColList},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The arena base: a Ptr32 word is an address minus this.
uint64_t tpw_bmp_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

uint64_t tpw_bmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bmp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, x));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, push));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, col));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, owner));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, counter));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMP));
    out[n++] = static_cast<int64_t>(sizeof(tabCOLOBJECT));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, first));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, count));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPOwner, scale));
    return n;
}

void tpw_bmp_inside(uint64_t bump) { BMP_InsideArea(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump))); }
void tpw_bmp_outside(uint64_t bump) { BMP_OutsideArea(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump))); }

}  // extern "C"
