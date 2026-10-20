// Test library for std::__cdeque<const void*>::swap (unit v5105; libtpw_unit_std___cdeque_swap.dylib), main app:
//   src/UI_pump_std___cdeque_swap.cpp
// No callees, no globals.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_cdeque.h"

extern "C" {

uint64_t tpw_cdq_addr(const char*) { return 0; }

uint64_t tpw_cdq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cdq_layout(int64_t* out) {
    using D = std::__cdeque<const void*, std::allocator<const void*>>;
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(D, mWord4));
    out[n++] = static_cast<int64_t>(offsetof(D, mWord8));
    out[n++] = static_cast<int64_t>(offsetof(D, mWordC));
    out[n++] = static_cast<int64_t>(sizeof(D));
    return n;
}

void tpw_cdq_swap(uint64_t a, uint64_t b) {
    using D = std::__cdeque<const void*, std::allocator<const void*>>;
    reinterpret_cast<D*>(a)->swap(*reinterpret_cast<D*>(b));
}

}  // extern "C"
