// Test library for std::__tree<nocase_string, ...> (a std::set<nocase_string>): erase, destroy, insert_node_at (unit v5899;
// libtpw_unit_std___tree_erase_2.dylib, tests/test_std___tree_erase_2.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are recording fakes: the runtime (new, new[], delete, strlen, strcpy, __throw), the string's copy
// constructor and destructor, the allocator's destroy and deallocate, and the red-black balancing (insert and erase). They write nothing
// except new/new[] which hand out zeroed memory (a host node is larger than the Mac's 0x10 bytes, so the size is logged as the Mac's).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MslSubscriptionTree.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];

static void* Fresh(uint32_t size) { return tpw::ArenaAllocZeroed(size < 64 ? 64 : size); }

void* MslNew(uint32_t size) {
    ev("new", {size == sizeof(NocaseSetTree::node) ? 0x10 : static_cast<int64_t>(size)});
    return Fresh(size);
}
void* MslNewArray(uint32_t size) {
    ev("newarray", {size});
    return Fresh(size);
}
void MslDelete(void* p) { ev("delete", {lab(p)}); }
size_t MslStrlen(const char* s) {
    ev("strlen", {}, {s});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {}, {s});
    return d;
}
void MslThrow(const char* typeName, void*, const void*) { ev("throw", {}, {typeName}); }
}  // namespace IME

void MslRedBlackTree::balance_erase(node_base* node, node_base* root) { ev("balance_erase", {lab(node), lab(root)}); }

namespace nocase_callee {
void AllocDestroy(void* allocator, nocase_string* value) { ev("alloc_destroy", {lab(allocator), lab(value)}); }
void Deallocate(void* allocator, void* node, unsigned long count) {
    ev("dealloc", {lab(allocator), lab(node), static_cast<int64_t>(count)});
}
void StringDtor(nocase_string* self, int16_t flag) { ev("string_dtor", {lab(self), flag}); }
nocase_string* StringCopy(nocase_string* self, const nocase_string* other) {
    ev("string_copy", {lab(self), lab(const_cast<nocase_string*>(other))});
    return self;
}
void BalanceInsert(NocaseSetTree::node* node, NocaseSetTree::node* root) { ev("balance_insert", {lab(node), lab(root)}); }
}  // namespace nocase_callee

extern "C" {

uint64_t tpw_nocase_addr(const char*) { return 0; }

uint64_t tpw_nocase_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_nocase_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree::node, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree::node, mRight));
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree::node, mParent));
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree::node, mValue));
    out[n++] = static_cast<int64_t>(offsetof(NocaseRep, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(NocaseRep, mData));
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree, mCount));
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(NocaseSetTree, mBegin));
    return n;
}

void tpw_nocase_erase(uint64_t tree, uint64_t position) {
    reinterpret_cast<NocaseSetTree*>(tree)->erase(reinterpret_cast<NocaseSetTree::node*>(position));
}

void tpw_nocase_destroy(uint64_t tree, uint64_t subtree) {
    reinterpret_cast<NocaseSetTree*>(tree)->destroy(reinterpret_cast<NocaseSetTree::node*>(subtree));
}

uint64_t tpw_nocase_insert(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint64_t key) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<NocaseSetTree*>(tree)->insert_node_at(
        reinterpret_cast<NocaseSetTree::node*>(parent), (left & 0xff) != 0, (leftmost & 0xff) != 0,
        *reinterpret_cast<const nocase_string*>(key))));
}

}  // extern "C"
