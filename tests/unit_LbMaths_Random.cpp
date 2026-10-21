// Test library for LbMaths_Random (unit v5738; libtpw_unit_LbMaths_Random.dylib, tests/test_LbMaths_Random.py), main app:
//   src/sound/AudioPlaceHolder.cpp   LbMaths_Random
//
// RotateRight<long> is a fake that records its two arguments.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AudioPlaceHolder.h"

using namespace calllog;

namespace SamsUtilities::UBinary {
template <>
int32_t RotateRight<int32_t>(int32_t value, int32_t amount) {
    ev("RotateRight", {value, amount});
    return static_cast<int32_t>(ret("RotateRight"));
}
}  // namespace SamsUtilities::UBinary

extern "C" {

uint64_t tpw_lbr_addr(const char*) { return 0; }

uint64_t tpw_lbr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lbr_layout(int64_t*) { return 0; }

// Answers the new seed; the seed is passed and written back through a pointer.
uint32_t tpw_lbr_random(uint64_t seedAddr) {
    return static_cast<uint32_t>(LbMaths_Random(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(seedAddr))));
}

}  // extern "C"
