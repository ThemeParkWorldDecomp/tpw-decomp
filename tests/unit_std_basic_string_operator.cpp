// Test library for std::basic_string<wchar_t>::operator[] (unit v5099; libtpw_unit_std_basic_string_operator.dylib,
// tests/test_std_basic_string_operator.py), main app: src/game/MslSupport.cpp.
// The callees (operator new, wmemcpy) are fakes that record their arguments.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
void* MslNewRep() {
    ev("new", {0x10});
    return tpw::ArenaAllocZeroed(sizeof(MslWideStringRep));
}
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
char16_t* MslWmemcpy(char16_t* d, const char16_t* s, uint32_t n) {
    ev("wmemcpy", {static_cast<int64_t>(n)});
    std::memcpy(d, s, n * 2);
    return d;
}
}  // namespace IME

extern "C" {

uint64_t tpw_bsop_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bsop_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideStringRep));
    return n;
}

uint64_t tpw_bsop_index(uint64_t self, uint32_t index) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        IME::MslWideString_index(reinterpret_cast<IME::MslWideString*>(static_cast<uintptr_t>(self)), index)));
}

}  // extern "C"
