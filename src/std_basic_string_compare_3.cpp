// std::basic_string<wchar_t, nocase_wchar_traits, std::allocator<wchar_t>>: compare 0x10225a5c (496 bytes) and the substring
// constructor 0x1022213c (628 bytes). The string object is one pointer to a shared representation (length, capacity, reference
// count, data pointer; IME::MslWideStringRep). Both functions raise std::out_of_range when pos > length; the throw fake returns
// and the code runs on (as the listing does after the call).
#include <cstdint>

#include "online.h"
#include "ui_ime.h"

namespace NS_MacDoze {
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count);   // glue 0x1023a660
}

namespace {

const char kOutOfRangeName[] = "!std::exception!!std::logic_error!!std::out_of_range!!";   // 0x10251824

// The inline construction of the exception object and the throw (strlen, new[], new, strcpy, __throw).
void ThrowOutOfRange(const char* message) {
    IME::MslExceptionObject ex;
    ex.mVtbl = IME::gMslExcVtblC;
    ex.mVtbl = IME::gMslExcVtblB;
    ex.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(message)) + 1));
    ex.mRc.mCount = nullptr;
    if (ex.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
        if (count != nullptr) *count = 1;
        ex.mRc.mCount = count;
    }
    IME::MslStrcpy(ex.mRc.mPtr, message);
    ex.mVtbl = IME::gMslExcVtblA;
    IME::MslThrow(kOutOfRangeName, &ex, IME::gMslOutOfRangeDtorTV);
}

IME::MslWideStringRep* RepOf(const void* self) { return *static_cast<IME::MslWideStringRep* const*>(self); }

}  // namespace

namespace online_callee {

// 0x10225a5c: compare(pos, n1, s, n2) const
int32_t NocaseWStringCompare(const void* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    if (pos > RepOf(self)->mLength) ThrowOutOfRange("string compare: pos > len");
    const IME::MslWideStringRep* rep = RepOf(self);
    uint32_t avail = rep->mLength - pos;
    uint32_t a = (n1 < avail) ? n1 : avail;
    uint32_t m = (n2 < a) ? n2 : a;
    int32_t result = NS_MacDoze::_wcsnicmp(RepOf(self)->mData + pos, s, m);
    if (result == 0) {
        if (a < n2) return -1;
        if (a == n2) return 0;
        return 1;
    }
    return result;
}

// 0x1022213c: basic_string(const basic_string& other, pos, count)
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    *static_cast<IME::MslWideStringRep**>(self) = RepOf(other);
    uint32_t length = RepOf(other)->mLength;
    if (pos > length) ThrowOutOfRange("string copy constructor: pos > str len");
    uint32_t n = length - pos;
    if (n > count) n = count;
    if (n == length) {
        IME::MslWideStringRep* rep = RepOf(other);
        if (rep->mRefs != 0) {
            rep->mRefs = rep->mRefs + 1;
        } else {
            IME::MslWideStringRep* copy = static_cast<IME::MslWideStringRep*>(IME::MslNewRep());
            if (copy != nullptr) {
                copy->mLength = rep->mLength;
                copy->mCapacity = (rep->mLength + 3) - (rep->mLength & 3);
                copy->mRefs = 1;
                copy->mData = static_cast<char16_t*>(IME::MslNew((copy->mCapacity + 1) * 2));
                IME::MslWmemcpy(copy->mData, rep->mData, copy->mLength + 1);
            }
            rep = copy;
        }
        *static_cast<IME::MslWideStringRep**>(self) = rep;
    } else {
        const char16_t* first = RepOf(other)->mData + pos;
        NocaseWStringInit(self, first, first + n);
    }
}

}  // namespace online_callee
