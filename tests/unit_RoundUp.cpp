// Test library for RoundUp and GetCoasterTrackStats (unit v5275; libtpw_unit_RoundUp.dylib, tests/test_RoundUp.py), main app:
//   src/game/rides/CoasterSpline.cpp, src/game/rides/CoasterTrain.cpp
//
// GetCoasterInstanceFromCObject is a fake that records its argument and answers the block the test sets with tpw_rup_setinst.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

namespace {
tag_sCoasterInstance* gInst = nullptr;
}

tag_sCoasterInstance* GetCoasterInstanceFromCObject(void* object) {
    ev("GetCoasterInstanceFromCObject", {lab(object)});
    return gInst;
}

extern "C" {

uint64_t tpw_rup_addr(const char*) { return 0; }

uint64_t tpw_rup_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_rup_setinst(uint64_t p) { gInst = reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(p)); }

// The host offsets of the fields the module writes by Mac offset, then the size of the instance.
int32_t tpw_rup_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mMarkedBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTrackLength));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTopHeight));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTopSpeed));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    return n;
}

int32_t tpw_rup_roundup(float value) { return RoundUp(value); }

void tpw_rup_stats(uint64_t object, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    GetCoasterTrackStats(reinterpret_cast<void*>(static_cast<uintptr_t>(object)), reinterpret_cast<float*>(static_cast<uintptr_t>(a)),
                         reinterpret_cast<float*>(static_cast<uintptr_t>(b)), reinterpret_cast<float*>(static_cast<uintptr_t>(c)),
                         reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(d)));
}

}  // extern "C"
