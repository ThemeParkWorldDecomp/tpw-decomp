// Test library for std::set<wchar_t>'s tree insert_node_at (unit v5653; libtpw_unit_std___tree_insert_node_at_2.dylib,
// tests/test_std___tree_insert_node_at_2.py), main app:
//   src/COnlinePerson_SetDest_std___tree_insert_node_at_2.cpp   WcharSetTree::insert_node_at
//
// The callees (strlen, strcpy, operator new, operator new[], __throw, balance_insert) are recording fakes; __throw returns. The library
// globals of other files the function names are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
char16_t gMslWcharNulA;
char16_t gMslWcharNulB;

size_t MslStrlen(const char* s) {
    ev("strlen", {}, {s});
    return std::strlen(s);
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {}, {s});
    std::strcpy(d, s);
    return d;
}

void* MslNew(uint32_t size) {
    ev("new", {size});
    void* p = tpw::ArenaAllocZeroed(size < 64 ? 64 : size);
    if (size == 0xe) reinterpret_cast<WcharSetTree::node*>(p)->mParent = static_cast<uintptr_t>(ret("newfill") & 1);
    return p;
}

void* MslNewArray(uint32_t size) {
    ev("newa", {size});
    return tpw::ArenaAllocZeroed(size < 64 ? 64 : size);
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    ev("throw", {}, {typeName});
    const MslExceptionObject* o = static_cast<const MslExceptionObject*>(object);
    int64_t sum = 0;
    if (o->mRc.mPtr != nullptr)
        for (const char* c = o->mRc.mPtr; *c != 0; ++c) sum += static_cast<unsigned char>(*c);
    ev("ThrowObj", {o->mVtbl == gMslLengthErrorVtbl, dtorTV == gMslLengthErrorDtorTV, o->mRc.mPtr != nullptr, o->mRc.mCount != nullptr,
                    o->mRc.mCount != nullptr ? *o->mRc.mCount : -1, sum});
}
}  // namespace IME

namespace WcharTreeCallee {
void BalanceInsert(WcharSetTree::node* node, WcharSetTree::node* root) {
    ev("balance", {lab(root), lab(reinterpret_cast<void*>(node->mParent & ~static_cast<uintptr_t>(1))), static_cast<int64_t>(node->mParent & 1),
                   lab(node->mLeft), lab(node->mRight), node->mValue});
}
}  // namespace WcharTreeCallee

extern "C" {

uint64_t tpw_wtree_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_wtree_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree, mCount));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree, mBegin));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mRight));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mParent));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mValue));
    return n;
}

uint64_t tpw_wtree_insert(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint32_t value) {
    char16_t v = static_cast<char16_t>(value);
    WcharSetTree::node* r = reinterpret_cast<WcharSetTree*>(tree)->insert_node_at(reinterpret_cast<WcharSetTree::node*>(parent), left != 0,
                                                                                  leftmost != 0, v);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
