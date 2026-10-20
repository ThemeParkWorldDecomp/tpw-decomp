// The CodeWarrior std::deque<errorthingy>: tear_down 0x101f4a74, copy constructor 0x101f4d14 and destroy 0x101f584c (unit v5680).
// Words are the Mac's 32-bit words; the host address of one is (gMslMacAddressHigh << 32) | word.
#include <cstdint>
#include <cstring>
#include "UI_pump.h"
#include "std_deque_errorthingy.h"

uint32_t gMslErrorDequeBlockSize = 0x40;   // 0x102b142c: records per block of a new deque
uint32_t gMslErrorDequeFill = 0;           // 0x102b1430: the word the block table is padded with (its address is passed to push_back)

namespace {

uint32_t* W(uint32_t word) { return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }
uint32_t Lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

volatile uint32_t gSink;   // the original reads the blocks it "destroys" (the loops have no other effect)

void Touch(uint32_t block, uint32_t count) {
    for (uint32_t p = block; p < block + count * 8; p += 8) gSink = 0;
}

}  // namespace

// 0x101f584c: destroys the records [first, last). errorthingy has no destructor: only the walk over the blocks remains.
void MslErrorDeque_destroy(MslErrorDeque* self, const MslErrorDequeIterator* first, const MslErrorDequeIterator* last) {
    if (first->mElement == last->mElement) return;
    uint32_t blockSize = self->mBlockSize;
    uint32_t index = first->mIndex;
    uint32_t total = last->mIndex - index;
    uint32_t blocks = blockSize != 0 ? index / blockSize : 0;   // the emulator answers 0 for divwu by 0
    uint32_t offset = index - blocks * blockSize;
    uint32_t span = offset + total;
    uint32_t limit = span < self->mBlockSize ? span : self->mBlockSize;

    MslErrorDequeCursor start;
    start.mCdeque = Lo(&self->mBlocks);
    start.mWord4 = 0;
    start.mSlot = self->mBlocks.mWordC + self->mBlocks.mWord8 * 4;
    MslErrorDequeCursor copy1;
    MslErrorDequeCursor* moved = errdeque_callee::CursorCopy(&copy1, &start);
    moved = MslErrorDequeCursorAddErrorthingy(moved, static_cast<int32_t>(blocks));
    MslErrorDequeCursor cursor;
    errdeque_callee::CursorCopy(&cursor, moved);

    uint32_t slot = cursor.mSlot;
    uint32_t block = *W(slot);
    Touch(block + offset * 8, limit - offset);
    uint32_t rest = total - (limit - offset);
    if (rest == 0) return;
    uint32_t* cdeque = W(cursor.mCdeque);
    uint32_t table = cdeque[3];
    uint32_t wrapEnd = table + cdeque[0] * 4;
    slot += 4;
    if (slot == wrapEnd) slot = table;
    for (;;) {
        uint32_t size = self->mBlockSize;
        if (rest < size) break;
        uint32_t b = *W(slot);
        Touch(b, size);
        slot += 4;
        if (slot == wrapEnd) slot = table;
        rest -= size;
    }
    if (rest == 0) return;
    Touch(*W(slot), rest);
}

// 0x101f4a74
void errdeque_callee::TearDown(MslErrorDeque* self) {
    if (self->mBlocks.mWord4 == 0) return;
    MslErrorDequeIterator endIt;
    MslErrorDequeIterator beginIt;
    errdeque_callee::MakeIteratorMutable(&endIt, self, self->mFirst + self->mSize);
    errdeque_callee::MakeIteratorMutable(&beginIt, self, self->mFirst);
    MslErrorDequeIterator endCopy = endIt;
    MslErrorDeque_destroy(self, &beginIt, &endCopy);

    uint32_t first = self->mBlocks.mWord8;
    uint32_t count = self->mBlocks.mWord4;
    uint32_t capacity = self->mBlocks.mWord0;
    uint32_t sum = first + count;
    uint32_t position = sum < capacity ? sum : sum - capacity;
    MslErrorDequeCursor cursor;
    cursor.mCdeque = Lo(&self->mBlocks);
    cursor.mWord4 = count;
    cursor.mSlot = self->mBlocks.mWordC + position * 4;
    MslErrorDequeCursor copy = cursor;
    errdeque_callee::CdequeDecrement(&copy);
    uint32_t n = copy.mWord4;

    uint32_t slot = self->mBlocks.mWordC + self->mBlocks.mWord8 * 4;
    for (uint32_t i = 0; i != n;) {
        errdeque_callee::OperatorDelete(W(*W(slot)));
        slot += 4;
        i++;
        uint32_t end = self->mBlocks.mWordC + self->mBlocks.mWord0 * 4;
        if (slot == end) slot = self->mBlocks.mWordC;
    }
}

// 0x101f4d14: the copy constructor
void errsinit_callee::DequeCopy(void* selfPtr, const void* otherPtr) {
    MslErrorDeque* self = static_cast<MslErrorDeque*>(selfPtr);
    const MslErrorDeque* other = static_cast<const MslErrorDeque*>(otherPtr);
    self->mBlockSize = gMslErrorDequeBlockSize;
    self->mBlocks.mWord0 = 0;
    self->mBlocks.mWord4 = 0;
    self->mBlocks.mWord8 = 0;
    self->mBlocks.mWordC = 0;
    self->mFirst = 0;
    self->mSize = other->mSize;
    if (self->mSize == 0) return;

    errdeque_callee::CdequeReserve(&self->mBlocks, 2);
    errdeque_callee::CdequeResize(&self->mBlocks, 1, nullptr);
    if (self->mBlockSize < self->mSize) self->mBlockSize = self->mSize;
    uint32_t block = Lo(errdeque_callee::OperatorNew(self->mBlockSize << 3));
    *W(self->mBlocks.mWordC + self->mBlocks.mWord8 * 4) = block;
    errdeque_callee::CdequePushBack(&self->mBlocks, &gMslErrorDequeFill);
    self->mFirst = (self->mBlockSize - self->mSize) >> 1;
    uint32_t dst = block + self->mFirst * 8;

    MslErrorDequeIterator endIt;
    MslErrorDequeIterator it;
    other->make_iterator(&endIt, other->mFirst + other->mSize);
    other->make_iterator(&it, other->mFirst);
    while (it.mElement != endIt.mElement) {
        if (dst != 0) {
            const uint32_t* src = W(it.mElement);
            uint32_t* d = W(dst);
            d[0] = src[0];
            d[1] = src[1];
        }
        it.mIndex++;
        dst += 8;
        it.mElement += 8;
        if (it.mElement == it.mBlockEnd) {
            uint32_t current = *W(it.mCursor.mSlot);
            int32_t records = static_cast<int32_t>(it.mBlockEnd - current) / 8;
            it.mCursor.mWord4++;
            uint32_t slot = it.mCursor.mSlot + 4;
            const uint32_t* cdeque = W(it.mCursor.mCdeque);
            if (slot == cdeque[3] + cdeque[0] * 4) slot = cdeque[3];
            it.mCursor.mSlot = slot;
            it.mElement = *W(slot);
            it.mBlockEnd = it.mElement + static_cast<uint32_t>(records) * 8;
        }
    }
}
