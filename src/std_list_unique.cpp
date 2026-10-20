// std::list<unsigned int>::unique 0x10208dcc and ::sort 0x10208c04 of the Mac (unit v5683). Words are 32 bit arena addresses.
#include <cstdint>

#include "platform/arena.h"
#include "std_list_uint.h"

namespace std_list_uint {

namespace {

// The words are offsets from the arena base (the Mac's 32 bit addresses).
Node* N(uint32_t w) { return reinterpret_cast<Node*>(tpw::ArenaBase() + w); }
uint32_t A(const void* p) { return tpw::ArenaOffset(p); }

}  // namespace

void Unique(List* list) {
    uint32_t sentinel = A(&list->mPrev);
    uint32_t cur = list->mNext;
    while (cur != sentinel) {
        uint32_t first = N(cur)->mNext;
        uint32_t f = first;
        while (f != sentinel && N(cur)->mData == N(f)->mData) f = N(f)->mNext;
        cur = first;
        if (first == f) continue;
        uint32_t lastDup = N(f)->mPrev;
        N(N(first)->mPrev)->mNext = N(lastDup)->mNext;
        N(N(lastDup)->mNext)->mPrev = N(first)->mPrev;
        uint32_t p = first;
        while (p != f) {
            uint32_t next = N(p)->mNext;
            OperatorDelete(N(p));
            list->mSize = list->mSize - 1;
            p = next;
        }
        cur = f;
    }
}

void Sort(List* list) {
    uint32_t size = list->mSize;
    if (size == 2) {
        Node* a = N(list->mNext);
        Node* b = N(a->mNext);
        if (b->mData < a->mData) {
            uint32_t t = a->mPrev;
            a->mPrev = a->mNext;
            a->mNext = t;
            t = b->mPrev;
            b->mPrev = b->mNext;
            b->mNext = t;
            t = list->mPrev;
            list->mPrev = list->mNext;
            list->mNext = t;
        }
    } else if (static_cast<int32_t>(size) > 2 || static_cast<int32_t>(size) < 0) {
        uint32_t half = size >> 1;
        uint32_t mid = list->mNext;
        for (uint32_t i = half; i != 0; --i) mid = N(mid)->mNext;
        // the local list lives in the arena: the host stack is not 32 bit addressable
        List* loc = static_cast<List*>(tpw::ArenaAllocZeroed(sizeof(List)));
        uint32_t s = A(&loc->mPrev);
        loc->mNext = s;
        loc->mPrev = s;
        uint32_t last = list->mPrev;
        N(N(mid)->mPrev)->mNext = N(last)->mNext;
        N(N(last)->mNext)->mPrev = N(mid)->mPrev;
        loc->mNext = mid;
        N(mid)->mPrev = loc->mPrev;
        loc->mPrev = last;
        N(last)->mNext = s;
        loc->mSize = list->mSize - half;
        list->mSize = half;
        Sort(list);
        Sort(loc);
        Merge(list, loc);
        if (loc->mSize != 0) {
            uint32_t p = loc->mNext;
            while (p != s) {
                uint32_t next = N(p)->mNext;
                OperatorDelete(N(p));
                p = next;
            }
        }
        tpw::ArenaFree(loc);
    }
}

}  // namespace std_list_uint
