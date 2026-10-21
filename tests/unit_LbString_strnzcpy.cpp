// Test library for LbString_strnzcpy (unit v5703; libtpw_unit_LbString_strnzcpy.dylib), main app:
//   src/bullfrog/LbString.cpp
// strlen and memcpy are the C library's on both sides.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbString.h"

extern "C" {

uint64_t tpw_sz_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sz_strnzcpy(uint64_t dst, uint64_t src, uint32_t size) {
    return LbString_strnzcpy(reinterpret_cast<char*>(static_cast<uintptr_t>(dst)),
                             reinterpret_cast<const char*>(static_cast<uintptr_t>(src)), size) & 0xff;
}

}  // extern "C"
