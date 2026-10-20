// Unit v5976: std::deque<std::vector<Argument>>::grow_buf_back 0x102287e4, ::destroy 0x10228fd4 and ::~deque 0x10213b80.
// The unwinding handler of grow_buf_back (0x102289e0 on: free the block, __throw, __end__catch) only runs when a callee throws; the
// runtime's operator new never fails, so it is not translated.
#include <cstdint>

#include "online.h"
#include "std_cdeque.h"

extern uint64_t gMslMacAddressHigh;

uint32_t gArgDequeGrowFill = 0;   // 0x102b2088

namespace {

using Cd = std::__cdeque<const void*, std::allocator<const void*>>;

// The address a Mac word names, plus index 4-byte elements.
uint32_t* At(uint32_t word, uint32_t index) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(((gMslMacAddressHigh << 32) | word) + static_cast<uint64_t>(index) * 4));
}

uint32_t WordOf(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

namespace argdeque_callee {

bool GrowBufBack(MslArgumentDeque* self, uint32_t from, void* spareObject, uint32_t* added, uint32_t to) {
    uint32_t* spare = static_cast<uint32_t*>(spareObject);
    uint32_t* table = &self->mCapacity;
    uint32_t remaining = to - from;
    bool swapped = false;
    for (;;) {
        uint32_t block = WordOf(online_callee::OperatorNew(self->mBlockSize * 0xc));
        if (!swapped && self->mCount == self->mCapacity) {
            if (table != spare) {
                for (int i = 0; i < 4; i++) {
                    uint32_t temp = table[i];
                    table[i] = spare[i];
                    spare[i] = temp;
                }
            }
            swapped = true;
            CdequeReserve(table, spare[0] * 2);
            uint32_t endSlot = spare[2] + spare[1];
            uint32_t endIndex = endSlot < spare[0] ? endSlot : endSlot - spare[0];
            MslCdequeIter last{reinterpret_cast<Cd*>(spare), spare[1], At(spare[3], endIndex)};
            MslCdequeIter first{reinterpret_cast<Cd*>(spare), 0, At(spare[3], spare[2])};
            CdequeDoAssignArgumentIter(reinterpret_cast<Cd*>(table), first, last);
        }
        uint32_t position = self->mCount - 1 + self->mStart;
        if (position >= self->mCapacity) position -= self->mCapacity;
        *At(self->mTable, position) = block;
        CdequePushBack(table, &gArgDequeGrowFill);
        *added = *added + 1;
        if (remaining <= self->mBlockSize) break;
        remaining -= self->mBlockSize;
    }
    return swapped;
}

}  // namespace argdeque_callee

void ArgumentDequeDestroy(MslArgumentDeque* self, MslArgumentDequeIter first, MslArgumentDequeIter last) {
    if (first.mElement == last.mElement) return;
    uint32_t blockSize = self->mBlockSize;
    uint32_t count = last.mIndex - first.mIndex;
    uint32_t blockNumber = blockSize != 0 ? first.mIndex / blockSize : 0;
    uint32_t offset = first.mIndex - blockNumber * blockSize;
    uint32_t total = offset + count;
    uint32_t end = total < blockSize ? total : blockSize;
    Cd* ring = reinterpret_cast<Cd*>(&self->mCapacity);
    MslCdequeIter start{ring, 0, At(self->mTable, self->mStart)};
    MslCdequeIter copy;
    cdeque_callee::IterCopy(&copy, &start);
    MslCdequeIter it;
    cdeque_callee::IterCopy(&it, cdeque_callee::IterAdd(&copy, static_cast<int32_t>(blockNumber)));
    uint32_t* slot = it.mPtr;
    uint32_t block = *slot;
    argdestroy_callee::DestroyRange(block + offset * 0xc, block + end * 0xc);
    uint32_t left = count - (end - offset);
    if (left == 0) return;
    slot++;
    if (slot == At(it.mDeque->mWordC, it.mDeque->mWord0)) slot = At(it.mDeque->mWordC, 0);
    while (left >= blockSize) {
        block = *slot;
        argdestroy_callee::DestroyRange(block, block + blockSize * 0xc);
        slot++;
        if (slot == At(it.mDeque->mWordC, it.mDeque->mWord0)) slot = At(it.mDeque->mWordC, 0);
        left -= blockSize;
    }
    if (left != 0) {
        block = *slot;
        argdestroy_callee::DestroyRange(block, block + left * 0xc);
    }
}

MslArgumentDeque* ArgumentDequeDestruct(MslArgumentDeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        online_callee::DequeTearDown(self);
        // The original also tests this + 4 against 0 twice; that only fails at the top of the Mac's address space.
        if (self->mTable != 0) online_callee::OperatorDelete(At(self->mTable, 0));
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}
