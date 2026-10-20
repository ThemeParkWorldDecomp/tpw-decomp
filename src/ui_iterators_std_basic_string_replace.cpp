// std::basic_string<wchar_t> of the Metrowerks library: replace (0x101f7740), substr (0x1021212c), begin (0x10212448).
// The string object holds one pointer to a shared representation (see MslWideStringRep in ui_ime.h).
#include <cstddef>
#include <cstdint>

#include "ui_ime.h"

namespace IME {
namespace {

MslWideStringRep* RepOf(const MslWideString* s) { return reinterpret_cast<MslWideStringRep*>(s->mData); }
void SetRep(MslWideString* s, MslWideStringRep* r) { s->mData = reinterpret_cast<char16_t*>(r); }

const char kOutOfRangeName[] = "!std::exception!!std::logic_error!!std::out_of_range!!";
const char kLengthErrorName[] = "!std::exception!!std::logic_error!!std::length_error!!";

// The inlined exception construction: the message copied into a counted buffer, then __throw (the fakes return).
// inlineCtor: replace builds the counted pointer by hand; substr calls _RefCountedPtr's constructor (0x100026ac).
void ThrowMsg(const char* typeName, const char* msg, const void* vtbl, const void* dtorTV, bool inlineCtor) {
    MslExceptionObject exc;
    exc.mVtbl = vtbl;
    size_t n = MslStrlen(msg);
    char* p = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    if (inlineCtor) {
        exc.mRc.mPtr = p;
        exc.mRc.mCount = nullptr;
        if (p != nullptr) {
            exc.mRc.mCount = static_cast<int32_t*>(MslNew(4));
            if (exc.mRc.mCount != nullptr) {
                *exc.mRc.mCount = 1;
            }
        }
    } else {
        MslRefCountedCharsCtor(&exc.mRc, p);
    }
    MslStrcpy(exc.mRc.mPtr, msg);
    MslThrow(typeName, &exc, dtorTV);
}

// The inlined representation constructor: capacity rounded to 3 mod 4, count 1, a data block of capacity + 1 characters.
MslWideStringRep* NewRep(uint32_t len) {
    MslWideStringRep* r = static_cast<MslWideStringRep*>(MslNewRep());
    if (r != nullptr) {
        r->mCapacity = (len + 3) - (len & 3);
        r->mRefs = 1;
        r->mData = static_cast<char16_t*>(MslNew((r->mCapacity + 1) * 2));
    }
    return r;
}

}  // namespace

// 0x101f7740. Returns this on the Mac (the declaration above says void).
void MslWideString_replace(MslWideString* self, uint32_t pos, uint32_t n1, uint32_t n2, char16_t c) {
    if (pos > RepOf(self)->mLength) {
        ThrowMsg(kOutOfRangeName, "basic_string::replace pos out of range", gMslExcVtblA, gMslOutOfRangeDtorTV, true);
    }
    uint32_t avail = RepOf(self)->mLength - pos;
    uint32_t eff = avail < n1 ? avail : n1;
    if (n2 > 0x7ffffffeu || RepOf(self)->mLength - eff > 0x7ffffffeu - n2) {
        ThrowMsg(kLengthErrorName, "basic_string::replace length error", gMslLengthErrorVtbl, gMslLengthErrorDtorTV, true);
    }
    MslWideStringRep* rep = RepOf(self);
    uint32_t len = rep->mLength;
    uint32_t newLen = n2 + (len - eff);
    if (rep->mRefs > 1 || newLen > rep->mCapacity) {
        MslWideStringRep* nr = NewRep(newLen);
        MslWmemcpy(nr->mData, RepOf(self)->mData, pos);
        if (n2 != 0) {
            MslWmemset(nr->mData + pos, c, n2);
        }
        rep = RepOf(self);
        MslWmemcpy(nr->mData + static_cast<uint32_t>(pos + n2), rep->mData + static_cast<uint32_t>(pos + eff),
                   rep->mLength - static_cast<uint32_t>(pos + eff));
        nr->mData[newLen] = gMslWcharNulA;
        nr->mLength = newLen;
        // Let go of the old representation: a count of 0 means unshared (one owner), then one owner less.
        rep = RepOf(self);
        if (rep->mRefs == 0) {
            rep->mRefs = 1;
        }
        rep->mRefs = rep->mRefs - 1;
        if (rep->mRefs != 0) {
            rep = nullptr;
        }
        SetRep(self, rep);
        if (RepOf(self) != nullptr) {
            MslDelete(RepOf(self)->mData);
            MslDelete(RepOf(self));
        }
        SetRep(self, nr);
    } else {
        uint32_t tail = len - static_cast<uint32_t>(pos + eff);
        MslWmemmove(rep->mData + static_cast<uint32_t>(pos + n2), rep->mData + static_cast<uint32_t>(pos + eff), tail);
        if (n2 != 0) {
            MslWmemset(RepOf(self)->mData + pos, c, n2);
        }
        RepOf(self)->mData[newLen] = gMslWcharNulB;
        RepOf(self)->mLength = newLen;
    }
}

// 0x1021212c. The result is built by the substring constructor.
MslWideString* MslWideString_substr(MslWideString* result, const MslWideString* self, uint32_t pos, uint32_t n) {
    if (pos > RepOf(self)->mLength) {
        ThrowMsg(kOutOfRangeName, "basic_string::substr pos out of range", gMslExcVtblA, gMslOutOfRangeDtorTV, false);
    }
    uint32_t avail = RepOf(self)->mLength - pos;
    uint32_t eff = avail < n ? avail : n;
    return MslWideString_construct(result, self, pos, eff);
}

// 0x10212448. Makes the string unshared (a private copy when shared), sets its count to 0 and answers the characters.
char16_t* MslWideString_begin(MslWideString* self) {
    if (RepOf(self)->mRefs > 1) {
        uint32_t len = RepOf(self)->mLength;
        MslWideStringRep* rep = RepOf(self);
        if (rep->mRefs > 1) {
            MslWideStringRep* nr = NewRep(len);
            rep->mRefs = rep->mRefs - 1;
            uint32_t cnt = rep->mLength < len ? rep->mLength : len;
            MslWmemcpy(nr->mData, rep->mData, cnt + 1);
            nr->mLength = rep->mLength;
            rep = nr;
        }
        SetRep(self, rep);
    }
    RepOf(self)->mRefs = 0;
    return RepOf(self)->mData;
}

}  // namespace IME
