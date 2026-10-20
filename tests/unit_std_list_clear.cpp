// Test library for std::list<CToken>::clear (unit v5132; libtpw_unit_std_list_clear.dylib, tests/test_std_list_clear.py), main app:
//   src/COnlineParser_ParseGoto_std_list_clear.cpp   CTokenListClear
//
// The only callee, operator delete (0x10238218), is a fake that records the label of the pointer.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

// ---- the fake
namespace slc_callee {
void OperatorDelete(void* p) { ev("Delete", {lab(p)}); }
}  // namespace slc_callee

extern "C" {

uint64_t tpw_slc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of a block: the word a Ptr32 holds.
uint32_t tpw_slc_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_slc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CTokenList));
    out[n++] = static_cast<int64_t>(sizeof(CTokenListNode));
    out[n++] = static_cast<int64_t>(sizeof(CTokenHandle));
    out[n++] = static_cast<int64_t>(offsetof(CTokenHandle, mCount));
    out[n++] = static_cast<int64_t>(offsetof(CTokenHandle, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(CTokenList, mPrev));
    return n;
}

void tpw_slc_clear(uint64_t list) { CTokenListClear(reinterpret_cast<CTokenList*>(static_cast<uintptr_t>(list))); }

}  // extern "C"
