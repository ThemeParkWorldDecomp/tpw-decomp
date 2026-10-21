// Test library for std::list<unsigned int>::erase and clear (unit v5305; libtpw_unit_std_list_erase.dylib, tests/test_std_list_erase.py), main app:
//   src/game/MslSupport.cpp   Erase, Clear
//
// operator delete (0x10238218) is a fake that records the data word of the node (nodes live in the 32-bit arena); other pointers are freed.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "std_list_uint.h"

using namespace calllog;

void std_list_uint::OperatorDelete(void* p) {
    ev("delete", {static_cast<int64_t>(*reinterpret_cast<uint32_t*>(static_cast<char*>(p) + 8))});
}

extern "C" {

uint64_t tpw_sl_addr(const char*) { return 0; }

uint64_t tpw_sl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_sl_off(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

uint64_t tpw_sl_ptr(uint32_t off) { return off == 0 ? 0 : static_cast<uint64_t>((uintptr_t)tpw::gArenaBase + off); }

int32_t tpw_sl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(std_list_uint::List));
    out[n++] = static_cast<int64_t>(offsetof(std_list_uint::List, mNext));
    out[n++] = static_cast<int64_t>(sizeof(std_list_uint::Node));
    return n;
}

void tpw_sl_erase(uint64_t result, uint64_t list, uint32_t first, uint32_t last) {
    std_list_uint::Erase(reinterpret_cast<uint32_t*>(result), reinterpret_cast<std_list_uint::List*>(list), first, last);
}

void tpw_sl_clear(uint64_t list) { std_list_uint::Clear(reinterpret_cast<std_list_uint::List*>(list)); }

}  // extern "C"
