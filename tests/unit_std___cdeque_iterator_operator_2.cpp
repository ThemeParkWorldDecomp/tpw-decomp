// Test library for the deque iterator operator+= (unit v5520), src/game/MslSupport.cpp. No callees.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include "calllog.h"
#include "platform/arena.h"
#include "std_cdeque.h"

uint32_t gCdequeMinCap;

extern "C" {

uint64_t tpw_cdit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// The deque's block word (mWordC) holds an offset from this on the host (a Ptr32), the raw address on the Mac.
uint64_t tpw_cdit_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

int32_t tpw_cdit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslCdequeIter, mDeque));
    out[n++] = static_cast<int64_t>(offsetof(MslCdequeIter, mIndex));
    out[n++] = static_cast<int64_t>(offsetof(MslCdequeIter, mPtr));
    out[n++] = static_cast<int64_t>(sizeof(MslCdequeIter));
    return n;
}

uint64_t tpw_cdit_add(uint64_t it, int32_t n) {
    return reinterpret_cast<uintptr_t>(cdeque_callee::IterAdd(reinterpret_cast<MslCdequeIter*>(it), n));
}

}  // extern "C"
