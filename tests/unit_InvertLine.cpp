// Test library for the text formatter's line helpers (unit v5757; libtpw_unit_InvertLine.dylib, tests/test_InvertLine.py).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbTextFormatter.h"

namespace tpw_v5757 {
void InvertLine(CCharacterInfo* first, CCharacterInfo* last);
}

extern "C" {

uint64_t tpw_il_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_il_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCharacterInfo, mChar));
    out[n++] = static_cast<int64_t>(offsetof(CCharacterInfo, mWidth));
    out[n++] = static_cast<int64_t>(sizeof(CCharacterInfo));
    return n;
}

void tpw_il_invert(uint64_t a, uint64_t b) {
    tpw_v5757::InvertLine(reinterpret_cast<CCharacterInfo*>(a), reinterpret_cast<CCharacterInfo*>(b));
}
uint32_t tpw_il_heb(uint64_t a, uint64_t b) {
    return IsHebInLine(reinterpret_cast<CCharacterInfo*>(a), reinterpret_cast<CCharacterInfo*>(b)) ? 1 : 0;
}
uint64_t tpw_il_nextnon(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(GetNextNonDelimiter(reinterpret_cast<CCharacterInfo*>(a), reinterpret_cast<CCharacterInfo*>(b)));
}
uint64_t tpw_il_nextdel(uint64_t a, uint64_t b) {
    return reinterpret_cast<uintptr_t>(GetNextDelimiter(reinterpret_cast<CCharacterInfo*>(a), reinterpret_cast<CCharacterInfo*>(b)));
}

}  // extern "C"
