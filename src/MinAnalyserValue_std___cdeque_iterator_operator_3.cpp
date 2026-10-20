#include <cstdint>
#include "History.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"

// The block words hold 32-bit Mac addresses; the host's heap lies above 4 GB (gMslMacAddressHigh, 0 on the Mac).
namespace {
uint32_t* BlockPtr(uint32_t word) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word));
}
uint32_t PtrWord(const uint32_t* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x101f5bdc: iterator::operator-=(long). A count of INT_MIN is the one value that reaches operator+=.
void MslCdequeIterSub(MslCdequeIter* it, int32_t n) {
    std::__cdeque<const void*, std::allocator<const void*>>* d = it->mDeque;
    if (n >= 0) {
        uint32_t un = static_cast<uint32_t>(n);
        if (un <= d->mWord8 + it->mIndex) {
            it->mPtr = BlockPtr(PtrWord(it->mPtr) - (un << 2));
        } else {
            it->mPtr = BlockPtr(d->mWordC + ((d->mWord0 - un) << 2));
        }
        it->mIndex = it->mIndex - un;
        return;
    }
    int32_t count = static_cast<int32_t>(0u - static_cast<uint32_t>(n));
    if (count < 0) {
        cdeque_callee::IterAdd(it, count);
        return;
    }
    uint32_t sum = it->mIndex + static_cast<uint32_t>(count) + d->mWord8;
    if (sum < d->mWord0) {
        it->mPtr = BlockPtr(PtrWord(it->mPtr) + (static_cast<uint32_t>(count) << 2));
    } else {
        it->mPtr = BlockPtr(d->mWordC + ((sum - d->mWord0) << 2));
    }
    it->mIndex = it->mIndex - static_cast<uint32_t>(n);
}

// 0x101f5200: queue::~queue (the queue is its deque at offset 0; the block table at 4).
MslErrorDeque* MslQueueErrorDestroy(MslErrorDeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        errdeque_callee::TearDown(self);
        errdeque_callee::QueueCdequeDtor(&self->mBlocks, -1);
        if (deleteFlag > 0) errdeque_callee::OperatorDelete(self);
    }
    return self;
}

// 0x101f2730
void std::__cdeque<const void*, std::allocator<const void*>>::reserve(uint32_t n) {
    using namespace IME;
    if (n > cdeque_callee::MaxSize(this)) {
        MslExceptionObject object;
        object.mVtbl = gMslExcVtblC;
        object.mVtbl = gMslExcVtblB;
        const char* message = "cdeque::reserve length error";
        size_t length = MslStrlen(message);
        object.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* counter = static_cast<int32_t*>(MslNew(4));
            if (counter != nullptr) *counter = 1;
            object.mRc.mCount = counter;
        }
        MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = gMslLengthErrorVtbl;
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, gMslLengthErrorDtorTV);
    }
    if (n > mWord0) {
        uint32_t* block = static_cast<uint32_t*>(MslNew(n << 2));
        if (mWordC != 0) {
            uint32_t* source = BlockPtr(mWordC) + mWord8;
            uint32_t* destination = block;
            for (uint32_t index = 0; index != mWord4; index++) {
                *destination = *source;
                source++;
                if (source == BlockPtr(mWordC) + mWord0) source = BlockPtr(mWordC);
                destination++;
            }
            if (mWordC != 0) MslDelete(BlockPtr(mWordC));
        }
        mWord0 = n;
        mWordC = PtrWord(block);
        mWord8 = 0;
    }
}
