// Test library for std::list<unsigned int>::unique and ::sort (unit v5683; libtpw_unit_std_list_unique.dylib, tests/test_std_list_unique.py), main app:
//   src/std_list_unique.cpp
//
// Fakes: merge (records both lists' sizes and values; drains the second list when the case's queue says so) and operator delete (records the node's value).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_list_uint.h"

using namespace calllog;
using namespace std_list_uint;

namespace std_list_uint {

static Node* NN(uint32_t w) { return reinterpret_cast<Node*>(tpw::ArenaBase() + w); }
static uint32_t AA(const void* p) { return tpw::ArenaOffset(p); }

void OperatorDelete(void* p) { ev("delete", {static_cast<int64_t>(static_cast<Node*>(p)->mData)}); }

void Merge(List* list, List* other) {
    int64_t v[12];
    int n = 0;
    v[n++] = list->mSize;
    v[n++] = other->mSize;
    for (const List* l : {list, other}) {
        uint32_t s = AA(&l->mPrev);
        uint32_t c = l->mNext;
        for (int i = 0; i < 5; ++i) {
            if (c != s) {
                v[n++] = NN(c)->mData;
                c = NN(c)->mNext;
            } else {
                v[n++] = 0x1FFFFFFFFll;
            }
        }
    }
    ev("merge", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]});
    if (ret("merge") != 0) {
        uint32_t s = AA(&other->mPrev);
        other->mSize = 0;
        other->mNext = s;
        other->mPrev = s;
    }
}

}  // namespace std_list_uint

extern "C" {

uint64_t tpw_sul_alloc(uint32_t size) { return tpw::ArenaOffset(tpw::ArenaAllocZeroed(size)); }
uint64_t tpw_sul_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

int32_t tpw_sul_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(List));
    out[n++] = static_cast<int64_t>(sizeof(Node));
    return n;
}

void tpw_sul_sort(uint64_t list) { Sort(reinterpret_cast<List*>(tpw::ArenaBase() + list)); }
void tpw_sul_unique(uint64_t list) { Unique(reinterpret_cast<List*>(tpw::ArenaBase() + list)); }

}  // extern "C"
