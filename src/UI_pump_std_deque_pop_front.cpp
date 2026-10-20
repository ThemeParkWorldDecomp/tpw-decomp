// Unit v5115: the CodeWarrior std::deque<errorthingy>: pop_front 0x101f2074, ~deque 0x101f52e0, make_iterator 0x101f53b4.
#include <cstdint>

#include "platform/arena.h"
#include "std_deque_errorthingy.h"

namespace {
// The deque's words are Mac addresses; on the host they are offsets from the arena base (like Ptr32), 0 staying null.
inline void* Ptr(uint32_t word) { return word != 0 ? static_cast<void*>(tpw::gArenaBase + word) : nullptr; }
inline uint32_t* Words(uint32_t word) { return static_cast<uint32_t*>(Ptr(word)); }
inline uint32_t Word(const void* address) { return address != nullptr ? tpw::ArenaOffset(address) : 0; }
}  // namespace

// Reads the block of the front record (the answer is dropped), moves the front on, and when the deque became empty lets go of every
// block but two (the table's last used slots, from the back) and recentres the front index. Bug kept: a block count under 2 loops
// about 2^32 times.
void MslErrorDeque::pop_front() {
    errdeque_callee::CdequeIndex(&mBlocks, mFirst / mBlockSize);
    mFirst = mFirst + 1;
    mSize = mSize - 1;
    if (mSize == 0) {
        uint32_t left = mBlocks.mWord4 - 2;
        while (left != 0) {
            mBlocks.mWord4 = mBlocks.mWord4 - 1;
            uint32_t index = mBlocks.mWord4 - 1 + mBlocks.mWord8;
            if (index >= mBlocks.mWord0) index = index - mBlocks.mWord0;
            uint32_t* slot = Words(mBlocks.mWordC) + index;
            errdeque_callee::OperatorDelete(Ptr(*slot));
            left = left - 1;
            *slot = 0;
        }
        mFirst = mBlockSize >> 1;
    }
}

// The iterator at `index`: inside the first block the record is read from the first slot's block (0 when the table is empty), past it
// the cursor is moved on by index / blockSize blocks.
void MslErrorDeque::make_iterator(MslErrorDequeIterator* out, uint32_t index) const {
    MslErrorDequeCursor cursor;
    cursor.mCdeque = Word(&mBlocks);
    cursor.mWord4 = 0;
    cursor.mSlot = mBlocks.mWordC + mBlocks.mWord8 * 4;
    uint32_t block;
    uint32_t element;
    if (index < mBlockSize) {
        block = mBlocks.mWord4 != 0 ? *Words(cursor.mSlot) : 0;
        element = block + index * 8;
    } else {
        errdeque_callee::CursorAdd(&cursor, static_cast<int32_t>(index / mBlockSize));
        block = *Words(cursor.mSlot);
        element = block + (index - index / mBlockSize * mBlockSize) * 8;
    }
    out->mCursor = cursor;
    out->mElement = element;
    out->mBlockEnd = block + mBlockSize * 8;
    out->mIndex = index;
}

// tear_down, then the block table (the member's destructor, guarded by two address tests that only fail at the top of the Mac's address
// space), then the object itself when the delete flag is positive.
MslErrorDeque* MslErrorDequeDestroy(MslErrorDeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        errdeque_callee::TearDown(self);
        const uint32_t member = Word(self) + 4;
        if (member != 0) {
            if (member != 0) {
                if (self->mBlocks.mWordC != 0) errdeque_callee::OperatorDelete(Ptr(self->mBlocks.mWordC));
            }
        }
        if (deleteFlag > 0) errdeque_callee::OperatorDelete(self);
    }
    return self;
}
