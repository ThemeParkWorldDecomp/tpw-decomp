// Unit v5938: the Metrowerks std::deque<std::vector<Argument>>: push_back 0x10224a3c, pop_front 0x1022488c, the copy constructor
// 0x10224fbc and make_iterator 0x10228c84. Elements are 0xc bytes (the Mac's vector); all words are the Mac's 32-bit words.
#include <cstdint>
#include <cstddef>

#include "online.h"
#include "ui_ime.h"

extern uint64_t gMslMacAddressHigh;

uint32_t gArgDequeBlockSize = 0x2a;   // 0x102b1f94
uint32_t gArgDequeFill = 0;           // 0x102b1f98

namespace {

void* Native(uint32_t word) { return reinterpret_cast<void*>(static_cast<uintptr_t>(gMslMacAddressHigh | word)); }
uint32_t Word(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
uint32_t* Table(const MslArgumentDeque* d) { return static_cast<uint32_t*>(Native(d->mTable)); }

}  // namespace

namespace online_callee {

void ArgumentDequePushBack(void* dequeObject, const void* vec) {
    MslArgumentDeque* self = static_cast<MslArgumentDeque*>(dequeObject);
    if (self->mSize > 0x15555554) {
        // deque::push_back length error; the library throws a std::length_error (the fake __throw returns and the code runs on)
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblB;
        const char* message = "deque::push_back length error";
        uint32_t length = static_cast<uint32_t>(IME::MslStrlen(message));
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(length + 1));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            object.mRc.mCount = count;
        }
        IME::MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    if (self->mCount == 0) {
        argdeque_callee::CdequeReserve(&self->mCapacity, 2);
        argdeque_callee::CdequeResize(&self->mCapacity, 1, nullptr);
    }
    uint32_t spare[4] = {0, 0, 0, 0};   // a std::__cdeque<const void*>
    uint32_t added = 0;
    if (self->mFirst + self->mSize == self->mBlockSize * (self->mCount - 1))
        argdeque_callee::GrowBufBack(self, 0, spare, &added, 1);
    uint32_t index = self->mFirst + self->mSize;
    uint32_t element;
    if (index < self->mBlockSize) {
        element = Table(self)[self->mStart] + index * 0xc;
        if (element != 0) argdeque_callee::VectorCopy(Native(element), vec);
    } else {
        uint32_t number = self->mBlockSize != 0 ? index / self->mBlockSize : 0;
        const uint32_t* slot = static_cast<const uint32_t*>(argdeque_callee::CdequeIndex(&self->mCapacity, number));
        uint32_t rest = index - (self->mBlockSize != 0 ? index / self->mBlockSize : 0) * self->mBlockSize;
        element = *slot + rest * 0xc;
        if (element != 0) argdeque_callee::VectorCopy(Native(element), vec);
    }
    self->mSize = self->mSize + 1;
    if (spare[3] != 0) IME::MslDelete(Native(spare[3]));
    while (self->mFirst >= (self->mBlockSize * 3) >> 1) {
        IME::MslDelete(Native(Table(self)[self->mStart]));
        argdeque_callee::CdequePopFront(&self->mCapacity);
        self->mFirst = self->mFirst - self->mBlockSize;
    }
}

void ArgumentDequePopFront(void* dequeObject) {
    MslArgumentDeque* self = static_cast<MslArgumentDeque*>(dequeObject);
    uint32_t number = self->mBlockSize != 0 ? self->mFirst / self->mBlockSize : 0;
    const uint32_t* slot = static_cast<const uint32_t*>(argdeque_callee::CdequeIndex(&self->mCapacity, number));
    uint32_t rest = self->mFirst - (self->mBlockSize != 0 ? self->mFirst / self->mBlockSize : 0) * self->mBlockSize;
    argdeque_callee::VectorDtor(Native(*slot + rest * 0xc), -1);
    self->mFirst = self->mFirst + 1;
    uint32_t size = self->mSize - 1;
    self->mSize = size;
    if (size == 0) {
        // empty: let go of every block but one (a count below 2 would loop 2^32 times, as on the Mac)
        for (uint32_t left = self->mCount - 2; left != 0; left = left - 1) {
            self->mCount = self->mCount - 1;
            uint32_t at = self->mCount - 1 + self->mStart;
            if (at >= self->mCapacity) at = at - self->mCapacity;
            uint32_t* entry = Table(self) + at;
            IME::MslDelete(Native(*entry));
            *entry = 0;
        }
        self->mFirst = self->mBlockSize >> 1;
    }
}

void DequeCopy(void* dequeObject, const void* otherObject) {
    MslArgumentDeque* self = static_cast<MslArgumentDeque*>(dequeObject);
    const MslArgumentDeque* other = static_cast<const MslArgumentDeque*>(otherObject);
    self->mBlockSize = gArgDequeBlockSize;
    self->mCapacity = 0;
    self->mCount = 0;
    self->mStart = 0;
    self->mTable = 0;
    self->mFirst = 0;
    self->mSize = other->mSize;
    if (self->mSize == 0) return;
    argdeque_callee::CdequeReserve(&self->mCapacity, 2);
    argdeque_callee::CdequeResize(&self->mCapacity, 1, nullptr);
    if (self->mBlockSize < self->mSize) self->mBlockSize = self->mSize;
    uint32_t block = Word(IME::MslNew(self->mBlockSize * 0xc));
    Table(self)[self->mStart] = block;
    argdeque_callee::CdequePushBack(&self->mCapacity, &gArgDequeFill);
    self->mFirst = (self->mBlockSize - self->mSize) >> 1;
    uint32_t target = block + self->mFirst * 0xc;
    MslArgumentDequeIter last;
    ArgumentDequeMakeIterator(&last, other, other->mFirst + other->mSize);
    MslArgumentDequeIter it;
    ArgumentDequeMakeIterator(&it, other, other->mFirst);
    while (argdeque_callee::IterNotEqual(&it, &last)) {
        // placement new (0x10201850 answers its second argument), then the vector copy constructor
        if (target != 0) argdeque_callee::VectorCopy(Native(target), argdeque_callee::IterDeref(&it));
        target = target + 0xc;
        argdeque_callee::IterInc(&it);
    }
}

}  // namespace online_callee

void ArgumentDequeMakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index) {
    MslArgumentCursor cursor;
    cursor.mCdeque = Word(&self->mCapacity);
    cursor.mIndex = 0;
    cursor.mSlot = self->mTable + self->mStart * 4;
    uint32_t base;
    uint32_t element;
    if (index < self->mBlockSize) {
        base = self->mCount == 0 ? 0 : *static_cast<uint32_t*>(Native(cursor.mSlot));
        element = base + index * 0xc;
    } else {
        argdeque_callee::CursorAdd(&cursor, static_cast<int32_t>(self->mBlockSize != 0 ? index / self->mBlockSize : 0));
        base = *static_cast<uint32_t*>(Native(cursor.mSlot));
        uint32_t rest = index - (self->mBlockSize != 0 ? index / self->mBlockSize : 0) * self->mBlockSize;
        element = base + rest * 0xc;
    }
    out->mCursor = cursor;
    out->mElement = element;
    out->mBlockEnd = base + self->mBlockSize * 0xc;
    out->mIndex = index;
}
