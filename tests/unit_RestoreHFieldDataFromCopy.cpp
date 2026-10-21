// Test library for RestoreHFieldDataFromCopy (unit v5287; libtpw_unit_RestoreHFieldDataFromCopy.dylib), main app:
//   src/game/park/TPHField.cpp
//
// No callees outside the C library's memcpy, which runs for real on both sides.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "m3d2.h"

using namespace calllog;

extern "C" {

// The arena offset a Ptr32 holds for a host address.
uint64_t tpw_rhf_addr(const char*) { return 0; }
uint32_t tpw_rhf_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(address)); }

uint64_t tpw_rhf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// offset of mHField in the header, of mQuadCount and mQuads in the field
int32_t tpw_rhf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mQuadCount));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mQuads));
    return n;
}

void tpw_rhf_call(uint64_t header, uint64_t copy) {
    RestoreHFieldDataFromCopy(reinterpret_cast<tag_sM3D2Header*>(header), reinterpret_cast<sHField*>(copy));
}

}  // extern "C"
