// Test library for the collision list point tests (unit v5112; libtpw_unit_PointOutAllInverse.dylib, tests/test_PointOutAllInverse.py), main app:
//   src/BMP_InsideArea_PointOutAllInverse.cpp   PointOutAllInverse, PointInAnyColObject, NavPointInArea
//
// PointInColObject is a fake that records x, y and the object's index in the list and answers from the test's queue.
// gColList (defined by another file) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"

using namespace calllog;

tabCOLLIST gColList;

int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object) {
    ev("PointInColObject", {x, y, static_cast<int64_t>(object - gColList.first.get())});
    return static_cast<int32_t>(ret("PointInColObject"));
}

extern "C" {

uint64_t tpw_pioai_addr(const char* name) {
    if (std::strcmp(name, "col") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gColList));
    return 0;
}

uint64_t tpw_pioai_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word that holds a host address (an offset from the arena base).
uint32_t tpw_pioai_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(address)); }

int32_t tpw_pioai_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, first));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, count));
    out[n++] = static_cast<int64_t>(sizeof(tabCOLOBJECT));
    return n;
}

uint32_t tpw_pioai_out_all(int32_t x, int32_t y) { return static_cast<uint32_t>(PointOutAllInverse(x, y)); }
uint64_t tpw_pioai_in_any(int32_t x, int32_t y) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(PointInAnyColObject(x, y))); }
uint64_t tpw_pioai_nav(uint64_t object, uint64_t out) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(NavPointInArea(reinterpret_cast<tabCOLOBJECT*>(object),
                                                                          reinterpret_cast<NS_MacDoze::tagPOINT*>(out))));
}

}  // extern "C"
