#include <cstdint>
#include <string>
#include "online.h"
#include "ui_ime.h"

// cmp_nocase(const std::wstring&, const std::wstring&, bool) 0x10210e68: compares with the characters below 0x100 mapped through
// __wupper_map; with the flag set, the end of the second string alone ends the comparison as equal (a prefix match).
static inline uint16_t UpperOf(uint16_t c) {
    return c < 0x100 ? gWUpperMap[c] : c;
}

static inline const IME::MslWideStringRep* RepOf(const IME::MslWideString& s) {
    // The string object is one pointer to its representation (the header types the word as char16_t*).
    return *reinterpret_cast<const IME::MslWideStringRep* const*>(&s);
}

int32_t cmp_nocase(const IME::MslWideString& a, const IME::MslWideString& b, bool prefix) {
    const IME::MslWideStringRep* ra = RepOf(a);
    const IME::MslWideStringRep* rb = RepOf(b);
    const char16_t* p = ra->mData;    // r30
    const char16_t* q = rb->mData;    // r3
    uint32_t lenA = ra->mLength;
    uint32_t lenB = rb->mLength;
    const char16_t* endA = ra->mData + lenA;
    const char16_t* endB = rb->mData + lenB;
    while (p != endA && q != endB) {
        uint16_t cb = *q;
        uint16_t ca = *p;
        if (UpperOf(ca) != UpperOf(cb)) return UpperOf(ca) < UpperOf(cb) ? -1 : 1;
        ++p;
        ++q;
    }
    if (prefix && q == endB) return 0;
    if (lenB == lenA) return 0;
    return lenA < lenB ? -1 : 1;
}
