#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>
#include "TbTextFormatter.h"

// 0x10006c3c: reverses [first, last) in place (same body as the file-local one in ltms_textformatter_rest.cpp).
namespace tpw_v5757 {
void InvertLine(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* back = last - 1; first < back; ++first, --back) std::swap(*first, *back);
}
}  // namespace tpw_v5757

// 0x10006a68: the first character of [first, last) that is a space or a comma; last when there is none.
CCharacterInfo* GetNextDelimiter(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* p = first; p < last; ++p)
        if (p->mChar == 0x20 || p->mChar == 0x2c) return p;
    return last;
}

// 0x10006b04: the first character of [first, last) that is neither a space nor a comma; NULL when there is none, or at once
// when first is past last - 2.
CCharacterInfo* GetNextNonDelimiter(CCharacterInfo* first, CCharacterInfo* last) {
    if (first > last - 2) return nullptr;
    for (CCharacterInfo* p = first; p < last; ++p)
        if (!(p->mChar == 0x20 || p->mChar == 0x2c)) return p;
    return nullptr;
}

// 0x10006bb4: true when a character of [first, last) lies strictly between 0x5b0 and 0x5eb (Hebrew letters).
bool IsHebInLine(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* p = first; p < last; ++p)
        if (p->mChar > 0x5b0 && p->mChar < 0x5eb) return true;
    return false;
}
