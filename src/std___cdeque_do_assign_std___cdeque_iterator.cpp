// std::__cdeque<const void*>::do_assign<std::__cdeque<errorthingy*>::iterator> 0x101f3278 (832 bytes): assigns the range [first, last)
// of the errorthingy block table to this block table. Word0 is the capacity, word4 the size, word8 the start, wordC the block.
#include <cstdint>

#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"

namespace {

// The Mac's word is the whole address; the host's heap lies above 4 GB, the upper half is gMslMacAddressHigh.
uint32_t* BlockOf(uint32_t word) { return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }

// The copy loop both branches run: one word per index, the source pointer wrapping at the end of the source's block.
void CopyRange(uint32_t* dst, const MslErrorDequeAssignIter& first, uint32_t lastIndex) {
    const std::__cdeque<const void*, std::allocator<const void*>>* src =
        static_cast<const std::__cdeque<const void*, std::allocator<const void*>>*>(first.mCdeque);
    uint32_t index = first.mIndex;
    uint32_t* cur = first.mSlot;
    while (index != lastIndex) {
        *dst = *cur;
        index = index + 1;
        cur = cur + 1;
        uint32_t* block = BlockOf(src->mWordC);
        if (cur == block + src->mWord0) cur = block;
        dst = dst + 1;
    }
}

}  // namespace

namespace errdeque_callee {

void CdequeDoAssign(void* cdeque, const MslErrorDequeAssignIter& first, const MslErrorDequeAssignIter& last) {
    std::__cdeque<const void*, std::allocator<const void*>>* self =
        static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(cdeque);
    uint32_t count = last.mIndex - first.mIndex;
    if (count > 0x3fffffffu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        const char* message = "cdeque::assign length error";
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(message) + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
            *object.mRc.mCount = 1;
        }
        IME::MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    if (count > self->mWord0) {
        uint32_t old = self->mWordC;
        void* block = IME::MslNew(count * 4);
        self->mWordC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block));
        CopyRange(BlockOf(self->mWordC), first, last.mIndex);
        if (old != 0) IME::MslDelete(BlockOf(old));
        self->mWord0 = count;
        self->mWord4 = count;
        self->mWord8 = 0;
    } else {
        CopyRange(BlockOf(self->mWordC), first, last.mIndex);
        self->mWord8 = 0;
        self->mWord4 = count;
    }
}

}  // namespace errdeque_callee
