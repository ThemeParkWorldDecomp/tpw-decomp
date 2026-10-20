// std::vector<Argument> copy constructor (0x10227200) and push_back (0x10201478). An Argument is three words (12 bytes).
// The exception paths (landing pads that destroy and rethrow) are unreachable here: the fakes of __throw return.
#include <cstdint>
#include <cstring>

#include "email.h"
#include "ui_ime.h"
#include "online.h"

using namespace std_vector_callee;

namespace {
const char kLengthErrorText[] = "vector::push_back length error";                       // 0x1024f8b2
const char kLengthErrorType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1024f9a0

inline Argument* At(Argument* base, uint32_t index) {
    return reinterpret_cast<Argument*>(reinterpret_cast<char*>(base) + static_cast<int32_t>(index * 12u));   // mulli wraps at 32 bits
}
}  // namespace

ArgumentVector::ArgumentVector(const ArgumentVector& other) {
    mCapacity = other.mSize;
    mSize = other.mSize;
    mData = nullptr;
    if (mCapacity != 0) {
        mData = static_cast<Argument*>(IME::MslNew(mCapacity * 12u));
        Argument* dst = mData;
        Argument* src = other.mData;
        Argument* end = At(other.mData, other.mSize);
        for (; src != end; src = At(src, 1)) {
            if (dst) ArgumentCopy(dst, src);   // placement new: the address itself
            dst = At(dst, 1);
        }
    }
}

void ArgumentVector::push_back(const Argument& argument) {
    if (mSize > 0x15555554u) {
        IME::MslExceptionObject exc;
        exc.mVtbl = IME::gMslExcVtblC;
        exc.mVtbl = IME::gMslExcVtblB;
        char* buffer = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kLengthErrorText)) + 1));
        IME::MslRefCountedCharsCtor(&exc.mRc, buffer);
        IME::MslStrcpy(exc.mRc.mPtr, kLengthErrorText);
        exc.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kLengthErrorType, &exc, IME::gMslLengthErrorDtorTV);
    }
    if (mSize < mCapacity) {
        Argument* slot = At(mData, mSize);
        if (slot) ArgumentCopy(slot, &argument);
        mSize = mSize + 1;
        return;
    }
    Argument* old = mData;
    uint32_t newCapacity = mCapacity == 0 ? 1u : mCapacity << 1;
    mData = static_cast<Argument*>(IME::MslNew(newCapacity * 12u));
    uint32_t count = mSize;
    Argument* dst = mData;
    Argument* src = old;
    Argument* end = At(old, count);
    for (; src != end; src = At(src, 1)) {
        if (dst) ArgumentCopy(dst, src);
        dst = At(dst, 1);
    }
    Argument* slot = At(mData, mSize);
    if (slot) ArgumentCopy(slot, &argument);
    if (old) {
        ArgumentDestroyRange(old, At(old, mSize));
        IME::MslDelete(old);
    }
    mSize = mSize + 1;
    mCapacity = newCapacity;
}
