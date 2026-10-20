// std::list<unsigned int, std::allocator<unsigned int>> of the Mac (CodeWarrior MSL), unit v5305. The words are 32 bit Mac addresses.
#pragma once
#include <cstdint>

namespace std_list_uint {

// A node: 12 bytes. The sentinel is the list's offset 4 (prev, next only).
struct Node {
    uint32_t mPrev;   // 0x0
    uint32_t mNext;   // 0x4
    uint32_t mData;   // 0x8
};

// The list: 12 bytes.
struct List {
    uint32_t mSize;   // 0x0
    uint32_t mPrev;   // 0x4  sentinel prev (the sentinel is at this + 4)
    uint32_t mNext;   // 0x8  sentinel next (the first node)
};

// std::list<unsigned_int,std::allocator<unsigned_int>>::erase 0x1020919c (188): r3 = where the returned iterator is stored, r4 = the list,
// r5 = first, r6 = last (iterators are the node addresses).
void Erase(uint32_t* result, List* list, uint32_t first, uint32_t last);
// operator delete glue 0x10238218 (weak in src/std_list_erase.cpp)
void OperatorDelete(void* p);
// std::list<unsigned_int,std::allocator<unsigned_int>>::clear 0x102090dc (124)
void Clear(List* list);

// Unit v5683 (src/std_list_unique.cpp)
// std::list<unsigned_int,std::allocator<unsigned_int>>::unique 0x10208dcc (308)
void Unique(List* list);
// std::list<unsigned_int,std::allocator<unsigned_int>>::sort 0x10208c04 (388)
void Sort(List* list);
// std::list<unsigned_int,std::allocator<unsigned_int>>::merge 0x102095a0 (652): stand-in, a fake in the test
void Merge(List* list, List* other);

// Unit v5931 (src/std_list_insert_2.cpp; merge above is defined there too)
// insert 0x102092f8 (428): result = where the iterator (the new node) is stored, pos = the node before which to insert
void Insert(uint32_t* result, List* list, uint32_t pos, const uint32_t* value);
// splice 0x1020a9a0 (432): moves all of other before pos
void Splice(List* list, uint32_t pos, List* other);
// remove 0x10208f44 (336)
void Remove(List* list, const uint32_t* value);

}  // namespace std_list_uint
