// std::basic_string<wchar_t> of the Metrowerks library (16-bit characters), main app:
//   compare 0x101d5864, assign 0x101f7014, substring constructor 0x101f7424.
// The string object is one pointer to a shared representation (see include/ui_ime.h). The Mac's exception landing pads
// (unreachable code after the inline throw sequences) are not translated. The fakes of __throw return, so the code after the
// throw runs on, as in the original.
#include <cstddef>
#include <cstdint>

#include "ui_ime.h"

namespace IME {

char gMslExcVtblA[16];        // 0x10278ba0
char gMslExcVtblB[16];        // 0x10278bb0
char gMslExcVtblC[16];        // 0x10278bc0
char gMslOutOfRangeDtorTV[8]; // 0x10265698

namespace {

inline MslWideStringRep*& RepOf(const MslWideString* s) {
    return *reinterpret_cast<MslWideStringRep**>(const_cast<MslWideString*>(s));
}

const char kOutOfRangeType[] = "!std::exception!!std::logic_error!!std::out_of_range!!";

// The throw sequence of compare: the counted message goes through the _RefCountedPtr constructor.
void ThrowCompare() {
    const char* msg = "string compare: pos > len";
    MslExceptionObject ex;
    ex.mVtbl = gMslExcVtblC;
    ex.mVtbl = gMslExcVtblB;
    size_t n = MslStrlen(msg);
    char* p = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    MslRefCountedCharsCtor(&ex.mRc, p);
    MslStrcpy(ex.mRc.mPtr, msg);
    ex.mVtbl = gMslExcVtblA;
    MslThrow(kOutOfRangeType, &ex, gMslOutOfRangeDtorTV);
}

// The inline throw sequence of assign and the constructor: the counter is built by hand.
void ThrowInline(const char* msg) {
    MslExceptionObject ex;
    ex.mVtbl = gMslExcVtblC;
    ex.mVtbl = gMslExcVtblB;
    size_t n = MslStrlen(msg);
    ex.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    ex.mRc.mCount = nullptr;
    if (ex.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(MslNew(4));
        if (count != nullptr) *count = 1;
        ex.mRc.mCount = count;
    }
    MslStrcpy(ex.mRc.mPtr, msg);
    ex.mVtbl = gMslExcVtblA;
    MslThrow(kOutOfRangeType, &ex, gMslOutOfRangeDtorTV);
}

// Shares a representation: one more reference, or (not shared yet) a copy with a count of 1.
MslWideStringRep* ShareRep(MslWideStringRep* r) {
    if (r->mRefs != 0) {
        r->mRefs = r->mRefs + 1;
        return r;
    }
    MslWideStringRep* p = static_cast<MslWideStringRep*>(MslNew(0x10));
    if (p != nullptr) {
        p->mLength = r->mLength;
        p->mCapacity = (r->mLength + 3) - (r->mLength & 3);
        p->mRefs = 1;
        p->mData = static_cast<char16_t*>(MslNew((p->mCapacity + 1) * 2));
        MslWmemcpy(p->mData, r->mData, p->mLength + 1);
    }
    return p;
}

}  // namespace

// 0x101d5864: compare(pos, n1, s, n2) const
int32_t MslWideString_compare(const MslWideString* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    if (pos > RepOf(self)->mLength) ThrowCompare();
    uint32_t avail = RepOf(self)->mLength - pos;
    uint32_t m1 = (n1 < avail) ? n1 : avail;
    uint32_t m2 = (n2 < m1) ? n2 : m1;
    int32_t r = MslWmemcmp(RepOf(self)->mData + pos, s, m2);
    if (r != 0) return r;
    if (m1 < n2) return -1;
    if (m1 == n2) return 0;
    return 1;
}

// 0x101f7014: assign(str, pos, n)
MslWideString* MslWideString_assign(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    if (pos > RepOf(str)->mLength) ThrowInline("basic_string::append pos out of range");
    uint32_t avail = RepOf(str)->mLength - pos;
    uint32_t cnt = (avail < n) ? avail : n;
    MslWideStringRep* r = RepOf(str);
    if (cnt == r->mLength && r->mRefs != 0) {
        MslWideStringRep* mine = RepOf(self);
        if (mine == r) return self;
        if (mine->mRefs == 0) mine->mRefs = 1;
        uint32_t v = mine->mRefs;
        mine->mRefs = v - 1;
        if (v - 1 != 0) mine = nullptr;
        RepOf(self) = mine;
        if (RepOf(self) != nullptr) {
            MslDelete(RepOf(self)->mData);
            MslDelete(RepOf(self));
        }
        MslWideStringRep* p = ShareRep(RepOf(str));
        RepOf(self) = p;
    } else if (self == str) {
        MslWideString_replace(self, 0, pos, 0, u'\0');
        MslWideString_changeSize(self, cnt, true);
    } else {
        MslWideString_changeSize(self, cnt, false);
        MslWmemcpy(RepOf(self)->mData, RepOf(str)->mData + pos, cnt);
    }
    return self;
}

// 0x101f7424: basic_string(str, pos, n)
MslWideString* MslWideString_construct(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    RepOf(self) = RepOf(str);
    uint32_t len = RepOf(str)->mLength;
    if (pos > len) ThrowInline("string copy constructor: pos > str len");
    uint32_t cnt = len - pos;
    if (cnt > n) cnt = n;
    if (cnt == len) {
        MslWideStringRep* p = ShareRep(RepOf(str));
        RepOf(self) = p;
    } else {
        const char16_t* data = RepOf(str)->mData;
        MslWideString_init(self, data + pos, data + pos + cnt);
    }
    return self;
}

}  // namespace IME
