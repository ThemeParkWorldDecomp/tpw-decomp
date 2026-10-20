// std::basic_string<wchar_t> of the Metrowerks library, main app: init 0x10146cf8, constructors 0x10146b8c (pointer, length),
// 0x10146a14 (C string), 0x101e0b84 (empty) and change_size 0x101b2cd8. The string is one pointer to a representation
// (length +0, capacity +4, reference count +8, data +0xc; the data pointer is native here). The addresses are computed
// in 32 bits like the Mac's, so a huge length wraps the same way. The fakes of __throw return, so the code goes on after
// the inline throw sequences.
#include "Cheat.h"
#include "CMessageCentre.h"
#include "LbFile.h"
#include "MsgCheatMode.h"
#include "platform/samsutils.h"

#include <cstddef>
#include <cstdint>

#include "platform/msl.h"
#include "ui_ime.h"

namespace IME {

char16_t gMslWcharNulInit = 0;    // 0x102a4d3c: the terminator init writes
char16_t gMslWcharNulResize = 0;  // 0x102ac356: the terminator change_size writes
char16_t gMslWcharNulEmpty = 0;   // 0x102b006c: the terminator of the empty string

namespace {

MslWideStringRep*& RepOf(MslWideString* s) { return reinterpret_cast<MslWideStringRep*&>(s->mData); }

// The address of data + index (in characters) with the Mac's 32-bit wrap.
char16_t* At(char16_t* data, uint32_t index) {
    return reinterpret_cast<char16_t*>(reinterpret_cast<uintptr_t>(data) + static_cast<intptr_t>(static_cast<int32_t>(index * 2u)));
}

// The exception object the library builds inline for std::length_error, then __throw (the fakes return).
void ThrowLengthError(const char* msg, const char* type) {
    MslExceptionObject ex;
    ex.mVtbl = gMslExcVtblC;
    ex.mVtbl = gMslExcVtblB;
    size_t n = MslStrlen(msg);
    char* p = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    MslRefCountedCharsCtor(&ex.mRc, p);
    MslStrcpy(ex.mRc.mPtr, msg);
    ex.mVtbl = gMslLengthErrorVtbl;
    MslThrow(type, &ex, gMslLengthErrorDtorTV);
}

const char kTypeName[] = "!std::exception!!std::logic_error!!std::length_error!!";

}  // namespace

// 0x10146cf8: init(first, last)
void MslWideString_init(MslWideString* self, const char16_t* first, const char16_t* last) {
    RepOf(self) = static_cast<MslWideStringRep*>(MslNewRep());
    int32_t d = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last)) -
                                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(first)));
    uint32_t n = static_cast<uint32_t>((d >> 1) + ((d < 0 && (d & 1) != 0) ? 1 : 0));
    MslWideStringRep* rep = RepOf(self);
    if (rep != nullptr) {
        rep->mCapacity = n + 3 - (n & 3);
        rep->mRefs = 1;
        rep->mData = static_cast<char16_t*>(MslNew((rep->mCapacity + 1) * 2));
    }
    MslWmemcpy(RepOf(self)->mData, first, n);
    RepOf(self)->mLength = n;
    *At(RepOf(self)->mData, n) = gMslWcharNulInit;
}

// 0x10146b8c: basic_string(const wchar_t*, unsigned long, const allocator&)
MslWideString* MslWideString_ctor(MslWideString* self, const char16_t* text, uint32_t n) {
    self->mData = nullptr;
    if (n > 0x7ffffffeu) ThrowLengthError("string constructor: n > max_size", kTypeName);
    MslWideString_init(self, text, At(const_cast<char16_t*>(text), n));
    return self;
}

// 0x10146a14: basic_string(const wchar_t*, const allocator&)
MslWideString* MslWideString_ctorCStr(MslWideString* self, const char16_t* text) {
    self->mData = nullptr;
    uint32_t n = wcslen(text);
    if (n > 0x7ffffffeu) ThrowLengthError("string constructor: n > max_size", kTypeName);
    MslWideString_init(self, text, At(const_cast<char16_t*>(text), n));
    return self;
}

// 0x101e0b84: basic_string(const allocator&): the empty string
MslWideString* MslWideString_ctorEmpty(MslWideString* self) {
    self->mData = nullptr;
    RepOf(self) = static_cast<MslWideStringRep*>(MslNewRep());
    MslWideStringRep* rep = RepOf(self);
    if (rep != nullptr) {
        rep->mCapacity = 3;
        rep->mRefs = 1;
        rep->mData = static_cast<char16_t*>(MslNew((rep->mCapacity + 1) * 2));
    }
    *RepOf(self)->mData = gMslWcharNulEmpty;
    RepOf(self)->mLength = 0;
    return self;
}

// 0x101b2cd8: change_size(n, keep)
void MslWideString_changeSize(MslWideString* self, uint32_t n, bool keep) {
    if (n > 0x7ffffffeu) {
        MslExceptionObject ex;
        ex.mVtbl = gMslExcVtblC;
        ex.mVtbl = gMslExcVtblB;
        const char msg[] = "basic_string::length error.  Resizing out of range";
        size_t len = MslStrlen(msg);
        ex.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(len + 1)));
        ex.mRc.mCount = nullptr;
        if (ex.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(MslNew(4));
            if (count != nullptr) *count = 1;
            ex.mRc.mCount = count;
        }
        MslStrcpy(ex.mRc.mPtr, msg);
        ex.mVtbl = gMslLengthErrorVtbl;
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &ex, gMslLengthErrorDtorTV);
    }
    if (RepOf(self)->mRefs > 1) {
        MslWideStringRep* old = RepOf(self);
        MslWideStringRep* fresh = static_cast<MslWideStringRep*>(MslNewRep());
        if (fresh != nullptr) {
            fresh->mCapacity = n + 3 - (n & 3);
            fresh->mRefs = 1;
            fresh->mData = static_cast<char16_t*>(MslNew((fresh->mCapacity + 1) * 2));
        }
        old->mRefs = old->mRefs - 1;
        if (keep) {
            uint32_t m = old->mLength < n ? old->mLength : n;
            MslWmemcpy(fresh->mData, old->mData, m + 1);
            fresh->mLength = old->mLength;
        }
        RepOf(self) = fresh;
    }
    MslWideStringRep* rep = RepOf(self);
    uint32_t cap = rep->mCapacity;
    if (n > cap) {
        uint32_t x = cap;
        while (x < n) x <<= 1;
        uint32_t want = x < rep->mLength ? rep->mLength : x;
        uint32_t newCap = want + 3 - (want & 3);
        if (newCap != rep->mCapacity) {
            char16_t* oldData = rep->mData;
            rep->mData = static_cast<char16_t*>(MslNew((newCap + 1) * 2));
            MslWmemcpy(rep->mData, oldData, rep->mLength + 1);
            MslDelete(oldData);
            rep->mCapacity = newCap;
        }
    }
    *At(RepOf(self)->mData, n) = gMslWcharNulResize;
    RepOf(self)->mLength = n;
    rep = RepOf(self);
    rep->mRefs = rep->mRefs == 0 ? 1 : rep->mRefs;
}

}  // namespace IME
