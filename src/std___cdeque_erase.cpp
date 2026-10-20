// std::__cdeque<const void*>: erase 0x101f3704, resize 0x101f2a10, push_back 0x101f459c.
// The elements are 4-byte words on the Mac; the block is addressed as uint32_t here.
#include "std_cdeque.h"
#include "Ptr32.h"
#include "ui_ime.h"

// The Mac's pointer words are offsets from tpw::gArenaBase here (0: null), as in the other 32-bit structures.
namespace {
using Cdeque = std::__cdeque<const void*, std::allocator<const void*>>;

inline uint32_t* BufOf(const Cdeque* d) {
    return d->mWordC != 0 ? reinterpret_cast<uint32_t*>(tpw::gArenaBase + d->mWordC) : nullptr;
}

// Step an iterator back one element (wrapping at the front of the block).
inline void StepBack(MslCdequeIter& it) {
    uint32_t* buf = BufOf(it.mDeque);
    if (it.mPtr == buf) it.mPtr = buf + it.mDeque->mWord0;
    it.mPtr -= 1;
}

// Step forward one element (wrapping at the end of the block).
inline void StepForward(MslCdequeIter& it) {
    it.mPtr += 1;
    uint32_t* buf = BufOf(it.mDeque);
    if (it.mPtr == buf + it.mDeque->mWord0) it.mPtr = buf;
}

inline MslCdequeIter IterAt(Cdeque* d, uint32_t index) {
    uint32_t pos = d->mWord8 + index;
    MslCdequeIter it;
    it.mDeque = d;
    it.mIndex = index;
    it.mPtr = pos < d->mWord0 ? BufOf(d) + pos : BufOf(d) + (pos - d->mWord0);
    return it;
}
}  // namespace

MslCdequeIter std::__cdeque<const void*, std::allocator<const void*>>::erase(MslCdequeIter first, MslCdequeIter last) {
    if (first.mIndex == last.mIndex) return first;
    uint32_t count = last.mIndex - first.mIndex;
    uint32_t after = mWord4 - last.mIndex;
    if (first.mIndex < after) {
        // fewer elements before the range: move them up, from the back
        if (first.mIndex != 0) {
            MslCdequeIter dst = last;
            MslCdequeIter src = first;
            while (src.mIndex != 0) {
                dst.mIndex -= 1;
                StepBack(dst);
                src.mIndex -= 1;
                StepBack(src);
                *dst.mPtr = *src.mPtr;
            }
        }
        mWord8 += count;
        if (mWord8 >= mWord0) mWord8 -= mWord0;
    } else if (after != 0) {
        // move the elements after the range down
        uint32_t size = mWord4;
        MslCdequeIter dst = first;
        MslCdequeIter src = last;
        while (src.mIndex != size) {
            *dst.mPtr = *src.mPtr;
            src.mIndex += 1;
            StepForward(src);
            dst.mIndex += 1;
            StepForward(dst);
        }
    }
    mWord4 -= count;
    return first;
}

void std::__cdeque<const void*, std::allocator<const void*>>::resize(uint32_t n, const void* value) {
    uint32_t size = mWord4;
    if (n > size) {
        MslCdequeIter pos = IterAt(this, size);
        cdeque_callee::Insert(this, pos, n - mWord4, &value);
    } else if (n < size) {
        MslCdequeIter end = IterAt(this, size);
        MslCdequeIter begin;
        begin.mDeque = this;
        begin.mIndex = 0;
        begin.mPtr = BufOf(this) + mWord8;
        MslCdequeIter t1, t2;
        MslCdequeIter* it = cdeque_callee::IterCopy(&t1, &begin);
        it = cdeque_callee::IterAdd(it, static_cast<int32_t>(n));
        cdeque_callee::IterCopy(&t2, it);
        erase(t2, end);
    }
}

void std::__cdeque<const void*, std::allocator<const void*>>::push_back(const void* const& value) {
    static const char kMessage[] = "cdeque::push_back length error";
    static const char kType[] = "!std::exception!!std::logic_error!!std::length_error!!";
    uint32_t element = value != nullptr ? tpw::ArenaOffset(value) : 0;
    if (mWord4 == 0x3fffffffu) {
        // the inlined length_error construction; the fakes of __throw return, so the code goes on afterwards
        IME::MslExceptionObject exc;
        exc.mVtbl = IME::gMslExcVtblB;
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage) + 1)));
        exc.mRc.mPtr = text;
        exc.mRc.mCount = nullptr;
        if (text != nullptr) {
            int32_t* counter = static_cast<int32_t*>(IME::MslNew(4));
            if (counter != nullptr) *counter = 1;
            exc.mRc.mCount = counter;
        }
        IME::MslStrcpy(exc.mRc.mPtr, kMessage);
        exc.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kType, &exc, IME::gMslLengthErrorDtorTV);
    }
    uint32_t cap = mWord0;
    uint32_t size = mWord4;
    if (size < cap) {
        uint32_t index = size + mWord8;
        if (index >= cap) index -= cap;
        BufOf(this)[index] = element;
        mWord4 = mWord4 + 1;
        return;
    }
    uint32_t base = cap < gCdequeMinCap ? gCdequeMinCap : cap;
    uint32_t newCap = base;
    if (size + 1 > base) newCap = base << 1;
    uint32_t* block = static_cast<uint32_t*>(IME::MslNew(newCap << 2));
    block[mWord4] = element;
    if (mWord4 != 0) {
        uint32_t* dst = block;
        MslCdequeIter src = IterAt(this, 0);
        src.mPtr = BufOf(this) + mWord8;
        uint32_t total = mWord4;
        while (src.mIndex != total) {
            *dst = *src.mPtr;
            src.mIndex += 1;
            StepForward(src);
            dst += 1;
        }
    }
    if (mWordC != 0) IME::MslDelete(BufOf(this));
    mWordC = tpw::ArenaOffset(block);
    mWord4 = mWord4 + 1;
    mWord0 = newCap;
    mWord8 = 0;
}
