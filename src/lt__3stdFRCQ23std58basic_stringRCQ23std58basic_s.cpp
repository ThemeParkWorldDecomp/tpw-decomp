// std::operator< for std::basic_string<wchar_t, nocase_wchar_traits>, main app, 0x10225d2c (140 B).
#include "online.h"

// 0x10225d2c: a.compare(0, a.size(), b.begin(), b.size()) < 0. The calls run in the listing's order: b.size(), b.begin(), a.size().
bool NocaseWStringLess(const void* a, const void* b) {
    uint32_t bSize = online_lt::NocaseWStringSize(b);
    const char16_t* bBegin = online_lt::NocaseWStringBegin(b);
    uint32_t aSize = online_lt::NocaseWStringSize(a);
    int32_t result = online_callee::NocaseWStringCompare(a, 0, aSize, bBegin, bSize);
    return result < 0;
}
