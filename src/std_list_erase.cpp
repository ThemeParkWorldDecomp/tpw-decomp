// std::list<unsigned int>::erase (0x1020919c) and clear (0x102090dc), unit v5305.
#include "std_list_uint.h"

#include <cstdint>
#include <new>

#include "platform/arena.h"

namespace std_list_uint {

namespace {
// The words are Ptr32 style: offsets from the arena base (include/Ptr32.h); the Mac's are addresses.
inline Node* N(uint32_t a) { return a != 0 ? reinterpret_cast<Node*>((uintptr_t)tpw::gArenaBase + a) : nullptr; }
inline uint32_t A(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

// operator delete (glue 0x10238218); weak so the unit test can replace it with a recording fake.
__attribute__((weak)) void OperatorDelete(void* p) { ::operator delete(p); }

void Erase(uint32_t* result, List* list, uint32_t first, uint32_t last) {
    if (first == last) {
        *result = last;
        return;
    }
    Node* l = N(last);
    Node* f = N(first);
    Node* lp = N(l->mPrev);
    N(f->mPrev)->mNext = lp->mNext;
    N(lp->mNext)->mPrev = f->mPrev;
    uint32_t cur = first;
    while (cur != last) {
        uint32_t next = N(cur)->mNext;
        OperatorDelete(N(cur));
        list->mSize = list->mSize - 1;
        cur = next;
    }
    *result = last;
}

void Clear(List* list) {
    if (list->mSize == 0) return;
    uint32_t cur = list->mNext;
    uint32_t sentinel = A(&list->mPrev);
    while (cur != sentinel) {
        uint32_t next = N(cur)->mNext;
        OperatorDelete(N(cur));
        cur = next;
    }
    list->mSize = 0;
    list->mNext = sentinel;
    list->mPrev = sentinel;
}

}  // namespace std_list_uint
