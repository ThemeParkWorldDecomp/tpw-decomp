// std::__cdeque<const void*>::iterator::operator+=(long) at 0x101f2bec (408 B).
#include <cstdint>

#include "platform/arena.h"
#include "std_cdeque.h"

namespace cdeque_callee {

// Words of the deque: 0 capacity, 4 size, 8 start, C block. Wraps around the block.
// mWordC is the block's address on the Mac; here a 32-bit offset from the arena base (Ptr32 rule).
// The original recurses forever for n == INT32_MIN (negation stays negative); not reproduced (it would overflow the stack).
MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n) {
    auto* d = it->mDeque;
    uint32_t* block = reinterpret_cast<uint32_t*>(tpw::ArenaBase() + d->mWordC);
    if (n >= 0) {
        uint32_t sum = d->mWord8 + it->mIndex + static_cast<uint32_t>(n);
        if (sum < d->mWord0)
            it->mPtr = it->mPtr + n;
        else
            it->mPtr = block + static_cast<int32_t>((sum - d->mWord0) << 2) / 4;  // slwi 2 wraps in 32 bits
        it->mIndex += static_cast<uint32_t>(n);
        return it;
    }
    if (n == INT32_MIN) return it;
    uint32_t m = static_cast<uint32_t>(-n);
    if (m > d->mWord8 + it->mIndex)
        it->mPtr = block + static_cast<int32_t>(d->mWord0 - m);  // the 32-bit result may be negative
    else
        it->mPtr = it->mPtr - m;
    it->mIndex += static_cast<uint32_t>(n);
    return it;
}

}  // namespace cdeque_callee
