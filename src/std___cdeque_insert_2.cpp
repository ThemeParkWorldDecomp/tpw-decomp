// std::__cdeque<const void*, std::allocator<const void*>>::insert(iterator pos, unsigned long count, const void* const& value)
// 0x101f3b10 (2564 B). Unit v51030. Word0 capacity, word4 size, word8 start, wordC block (offset from the arena base) of 4-byte elements.
#include <cstdint>

#include "platform/arena.h"
#include "std_cdeque.h"
#include "ui_ime.h"

namespace {

using Deque = std::__cdeque<const void*, std::allocator<const void*>>;

uint32_t* Blk(const Deque* d) { return reinterpret_cast<uint32_t*>(tpw::ArenaBase() + d->mWordC); }

// The element pointer for logical position sum (start + index): wrapped once when it reaches the capacity.
uint32_t* At(const Deque* d, uint32_t sum) {
    if (sum < d->mWord0) return Blk(d) + sum;
    return Blk(d) + (sum - d->mWord0);
}

// Fills count elements from p (wrapping at the end of the block of d).
void Fill(const Deque* d, uint32_t* p, uint32_t count, const uint32_t* value) {
    for (; count != 0; count--) {
        *p = *value;
        p++;
        if (p == Blk(d) + d->mWord0) p = Blk(d);
    }
}

}  // namespace

namespace cdeque_callee {

void Insert(Deque* self, MslCdequeIter pos, uint32_t count, const void* const* valueArg) {
    const uint32_t* value = reinterpret_cast<const uint32_t*>(valueArg);
    if (count == 0) return;
    const uint32_t kMax = 0x3fffffffu;
    if (count > kMax || self->mWord4 > kMax - count) {
        IME::MslExceptionObject obj;
        obj.mVtbl = IME::gMslExcVtblC;
        obj.mVtbl = IME::gMslExcVtblB;
        const char* msg = "cdeque::insert length error";
        uint32_t len = static_cast<uint32_t>(IME::MslStrlen(msg));
        obj.mRc.mPtr = static_cast<char*>(IME::MslNewArray(len + 1));
        obj.mRc.mCount = nullptr;
        if (obj.mRc.mPtr != nullptr) {
            int32_t* counter = static_cast<int32_t*>(IME::MslNew(4));
            if (counter != nullptr) *counter = 1;
            obj.mRc.mCount = counter;
        }
        IME::MslStrcpy(obj.mRc.mPtr, msg);
        obj.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &obj, IME::gMslLengthErrorDtorTV);
    }
    uint32_t cap = self->mWord0;
    uint32_t size = self->mWord4;
    uint32_t n = size + count;
    uint32_t idx = pos.mIndex;
    if (n > cap) {
        const uint32_t* from = cap >= gCdequeMinCap ? &self->mWord0 : &gCdequeMinCap;
        uint32_t newCap = *from;
        while (n > newCap) newCap <<= 1;
        uint32_t* nb = static_cast<uint32_t*>(IME::MslNew(newCap << 2));
        uint32_t* dst = nb;
        if (idx != 0) {
            uint32_t* s = Blk(self) + self->mWord8;
            for (uint32_t counter = 0; counter != idx;) {
                *dst = *s;
                counter++;
                s++;
                if (s == Blk(self) + self->mWord0) s = Blk(self);
                dst++;
            }
        }
        uint32_t* f = nb + idx;
        for (uint32_t c = count; c != 0; c--) {
            *f = *value;
            f++;
        }
        if (idx < size) {
            dst = nb + idx + count;
            uint32_t counter = pos.mIndex;
            uint32_t* s = pos.mPtr;
            while (counter != size) {
                *dst = *s;
                counter++;
                s++;
                if (s == Blk(pos.mDeque) + pos.mDeque->mWord0) s = Blk(pos.mDeque);
                dst++;
            }
        }
        if (self->mWordC != 0) IME::MslDelete(Blk(self));
        self->mWordC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(nb) - reinterpret_cast<uintptr_t>(tpw::ArenaBase()));
        self->mWord4 = n;
        self->mWord0 = newCap;
        self->mWord8 = 0;
        return;
    }
    uint32_t distance = size - idx;
    if (idx < distance) {
        uint32_t start = self->mWord8;
        if (start >= count)
            self->mWord8 = start - count;
        else
            self->mWord8 = self->mWord0 - (count - start);
        if (idx == 0) {
            Fill(self, Blk(self) + self->mWord8, count, value);
        } else {
            MslCdequeIter it0{self, 0, Blk(self) + self->mWord8};
            MslCdequeIter a, last, b, srcIt, c, fillIt;
            cdeque_callee::IterCopy(&last, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&a, &it0), count + idx));
            cdeque_callee::IterCopy(&srcIt, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&b, &it0), count));
            MslCdequeIter d = it0;
            while (srcIt.mIndex != last.mIndex) {
                *d.mPtr = *srcIt.mPtr;
                srcIt.mIndex++;
                srcIt.mPtr++;
                if (srcIt.mPtr == Blk(srcIt.mDeque) + srcIt.mDeque->mWord0) srcIt.mPtr = Blk(srcIt.mDeque);
                d.mIndex++;
                d.mPtr++;
                if (d.mPtr == Blk(d.mDeque) + d.mDeque->mWord0) d.mPtr = Blk(d.mDeque);
            }
            cdeque_callee::IterCopy(&fillIt, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&c, &it0), idx));
            Fill(fillIt.mDeque, fillIt.mPtr, count, value);
        }
    } else if (distance == 0) {
        Fill(self, At(self, self->mWord8 + self->mWord4), count, value);
    } else {
        MslCdequeIter e1{self, self->mWord4, At(self, self->mWord8 + self->mWord4)};
        MslCdequeIter a, ne;
        cdeque_callee::IterCopy(&ne, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&a, &e1), count));
        MslCdequeIter e2{self, self->mWord4, At(self, self->mWord8 + self->mWord4)};
        while (e2.mIndex != pos.mIndex) {
            ne.mIndex--;
            if (ne.mPtr == Blk(ne.mDeque)) ne.mPtr = Blk(ne.mDeque) + ne.mDeque->mWord0;
            ne.mPtr--;
            e2.mIndex--;
            if (e2.mPtr == Blk(e2.mDeque)) e2.mPtr = Blk(e2.mDeque) + e2.mDeque->mWord0;
            e2.mPtr--;
            *ne.mPtr = *e2.mPtr;
        }
        Fill(pos.mDeque, pos.mPtr, count, value);
    }
    self->mWord4 = self->mWord4 + count;
}

}  // namespace cdeque_callee
