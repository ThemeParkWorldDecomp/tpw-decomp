// Test library for std::vector<facet*>::~vector (unit v5577), src/game/MslSupport.cpp.
// operator delete is a recording fake: 1 = the vector, 2 = its buffer, 0 = other.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace {
uintptr_t gVec, gBuf;
}

namespace locale_imp_callee {
void Delete(void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    ev("Delete", {v == gVec ? 1 : (v == gBuf ? 2 : 0)});
}
}  // namespace locale_imp_callee

extern "C" {

uint64_t tpw_vd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_vd_layout(int64_t* out) {
    out[0] = 0;
    return 0;
}

uint64_t tpw_vd_dtor(uint64_t vec, int32_t flag) {
    gVec = static_cast<uintptr_t>(vec);
    uintptr_t b = 0;
    if (vec) std::memcpy(&b, reinterpret_cast<uint8_t*>(gVec) + 8, sizeof b);
    gBuf = b;
    void* r = locale_imp_callee::VectorFacetDtor(reinterpret_cast<void*>(gVec), static_cast<int16_t>(flag));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
