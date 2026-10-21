// Test library for std::ctype<wchar_t>::do_is (unit v5735; libtpw_unit_std_ctype_do_is.dylib, tests/test_std_ctype_do_is.py), main app:
//   src/game/MslSupport.cpp   do_is
//
// classify is a recording fake answering from the test's queue.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_ctype_wchar.h"

using namespace calllog;

uint32_t StdCtypeWchar::classify(char16_t c) const {
    ev("classify", {lab(const_cast<StdCtypeWchar*>(this)), static_cast<int64_t>(c)});
    return static_cast<uint32_t>(ret("classify"));
}

extern "C" {

uint64_t tpw_ctype_addr(const char*) { return 0; }

uint64_t tpw_ctype_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ctype_layout(int64_t*) { return 0; }

uint32_t tpw_ctype_do_is(uint64_t self, uint32_t m, uint32_t c) {
    return reinterpret_cast<const StdCtypeWchar*>(static_cast<uintptr_t>(self))->do_is(m, static_cast<char16_t>(c));
}

}  // extern "C"
