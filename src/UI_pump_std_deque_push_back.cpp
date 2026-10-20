#include <cstddef>
#include <cstdint>
#include <cstring>

#include "std_deque_errorthingy.h"
#include "ui_ime.h"

// std::deque<errorthingy> of the Metrowerks library: push_back and the block-table growth. All words are the Mac's 32-bit words;
// block pointers are kept as 32-bit values (the heap of the host tests is 32-bit addressable).
uint32_t gMslErrorDequePad = 0;   // 0x102b141c

namespace {
using Cd = std::__cdeque<const void*, std::allocator<const void*>>;

// A 32-bit Mac address as a host pointer (the upper half is gMslMacAddressHigh; 0 stays null).
uint32_t* Native(uint32_t word) {
    return word == 0 ? nullptr : reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word));
}
uint32_t* TableOf(const Cd& cd) { return Native(cd.mWordC); }
}  // namespace

// 0x101f2e7c: adds blocks until `to - from` records are covered. The block is allocated before the table is looked at; a full table
// is swapped with `spare`, grown and refilled from it (once); the new block replaces the last table slot, then the table gets one more
// word. Answers whether the swap happened.
uint32_t MslErrorDeque_grow_buf_back(MslErrorDeque* self, uint32_t from, Cd* spare, uint32_t* added, uint32_t to) {
    uint32_t remaining = to - from;
    uint32_t swapped = 0;
    for (;;) {
        void* block = IME::MslNew(self->mBlockSize << 3);
        Cd& mine = self->mBlocks;
        if (swapped == 0 && mine.mWord4 == mine.mWord0) {
            if (&mine != spare) {
                uint32_t t = mine.mWord0;
                mine.mWord0 = spare->mWord0;
                spare->mWord0 = t;
                t = mine.mWord4;
                mine.mWord4 = spare->mWord4;
                spare->mWord4 = t;
                t = mine.mWord8;
                mine.mWord8 = spare->mWord8;
                spare->mWord8 = t;
                t = mine.mWordC;
                mine.mWordC = spare->mWordC;
                spare->mWordC = t;
            }
            swapped = 1;
            errdeque_callee::CdequeReserve(&mine, spare->mWord0 << 1);
            uint32_t pos = spare->mWord8 + spare->mWord4;
            uint32_t* tab = TableOf(*spare);
            MslErrorDequeAssignIter last;
            last.mCdeque = spare;
            last.mIndex = spare->mWord4;
            last.mSlot = pos < spare->mWord0 ? tab + pos : tab + (pos - spare->mWord0);
            MslErrorDequeAssignIter first;
            first.mCdeque = spare;
            first.mIndex = 0;
            first.mSlot = tab + spare->mWord8;
            errdeque_callee::CdequeDoAssign(&mine, first, last);
        }
        uint32_t index = mine.mWord4 - 1 + mine.mWord8;
        if (index >= mine.mWord0) index -= mine.mWord0;
        TableOf(mine)[index] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block));
        errdeque_callee::CdequePushBack(&mine, &gMslErrorDequePad);
        *added = *added + 1;
        if (remaining <= self->mBlockSize) break;
        remaining -= self->mBlockSize;
    }
    return swapped;
}

// 0x101f21ac: the length check throws (the fakes of __throw return, so the code goes on), an empty block table gets two slots and one
// entry, the table grows when the last block is full, the record is copied, and the leading blocks are released once a block and a
// half lies in front of the first record.
void MslErrorDeque_push_back(MslErrorDeque* self, const uint32_t* record) {
    if (self->mSize > 0x1ffffffe) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblB;
        const char* message = "deque::push_back length error";
        uint32_t length = static_cast<uint32_t>(IME::MslStrlen(message));
        char* text = static_cast<char*>(IME::MslNewArray(length + 1));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    if (self->mBlocks.mWord4 == 0) {
        errdeque_callee::CdequeReserve(&self->mBlocks, 2);
        errdeque_callee::CdequeResize(&self->mBlocks, 1, nullptr);
    }
    Cd spare;
    spare.mWord0 = 0;
    spare.mWord4 = 0;
    spare.mWord8 = 0;
    spare.mWordC = 0;
    uint32_t added = 0;
    if (self->mFirst + self->mSize == self->mBlockSize * (self->mBlocks.mWord4 - 1))
        MslErrorDeque_grow_buf_back(self, 0, &spare, &added, 1);
    uint32_t index = self->mFirst + self->mSize;
    uint32_t element;
    if (index < self->mBlockSize) {
        element = TableOf(self->mBlocks)[self->mBlocks.mWord8] + index * 8;
    } else {
        uint32_t number = self->mBlockSize != 0 ? index / self->mBlockSize : 0;
        const uint32_t* slot = static_cast<const uint32_t*>(errdeque_callee::CdequeIndex(&self->mBlocks, number));
        uint32_t rest = index - (self->mBlockSize != 0 ? index / self->mBlockSize : 0) * self->mBlockSize;
        element = *slot + rest * 8;
    }
    if (element != 0) {
        uint32_t* target = Native(element);
        uint32_t w0 = record[0];
        uint32_t w1 = record[1];
        target[0] = w0;
        target[1] = w1;
    }
    self->mSize = self->mSize + 1;
    if (spare.mWordC != 0) errdeque_callee::OperatorDelete(Native(spare.mWordC));
    while (self->mFirst >= (self->mBlockSize * 3) >> 1) {
        errdeque_callee::OperatorDelete(Native(TableOf(self->mBlocks)[self->mBlocks.mWord8]));
        errdeque_callee::CdequePopFront(&self->mBlocks);
        self->mFirst = self->mFirst - self->mBlockSize;
    }
}
