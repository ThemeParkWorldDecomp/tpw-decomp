// Test library for cmp_nocase (unit v5568; libtpw_unit_cmp_nocase.dylib, tests/test_cmp_nocase.py), main app:
//   src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase.cpp   cmp_nocase
//
// No callees. The global of another file the unit reads (__wupper_map) is defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

uint16_t gWUpperMap[256];

extern "C" {

uint64_t tpw_cmpn_addr(const char* name) {
    if (std::strcmp(name, "map") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gWUpperMap[0]));
    return 0;
}

uint64_t tpw_cmpn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cmpn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideStringRep));
    return n;
}

int32_t tpw_cmpn_cmp(uint64_t a, uint64_t b, uint32_t flag) {
    return cmp_nocase(*reinterpret_cast<const IME::MslWideString*>(static_cast<uintptr_t>(a)),
                      *reinterpret_cast<const IME::MslWideString*>(static_cast<uintptr_t>(b)), flag != 0);
}

}  // extern "C"
