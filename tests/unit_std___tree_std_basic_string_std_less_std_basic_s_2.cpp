// Test library for WStringTree::erase_one (unit v5701). The less functor and erase are recording fakes; a string's first word is its key.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MslWStringTree.h"

using namespace calllog;

static int64_t KeyOf(const void* p) { return *static_cast<const uint32_t*>(p); }

bool WideStringLess(void* functor, const MslWideString* a, const MslWideString* b) {
    ev("Less", {lab(functor), KeyOf(a), KeyOf(b)});
    return static_cast<uint32_t>(KeyOf(a)) < static_cast<uint32_t>(KeyOf(b));
}

void WStringTree::erase(node_base* position) {
    ev("Erase", {lab(this), KeyOf(reinterpret_cast<const uint8_t*>(position) + sizeof(node_base))});
}

extern "C" {

uint64_t tpw_wt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_wt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(WStringTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(WStringTree, mLess));
    out[n++] = static_cast<int64_t>(sizeof(WStringTree::node_base));
    return n;
}

uint32_t tpw_wt_erase_one(uint64_t tree, uint64_t key) {
    return reinterpret_cast<WStringTree*>(static_cast<uintptr_t>(tree))->erase_one(*reinterpret_cast<const MslWideString*>(static_cast<uintptr_t>(key)));
}

}  // extern "C"
