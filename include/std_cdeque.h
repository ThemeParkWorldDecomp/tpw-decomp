#pragma once
#include <cstdint>
#include <memory>

// Unit v5105: the CodeWarrior deque base for const void* (16 bytes on the Mac: four words).
namespace std {
template <class T, class A>
struct __cdeque;
template <>
struct __cdeque<const void*, allocator<const void*>>;
}  // namespace std

// Unit v5149: the iterator of the deque (Mac 12 bytes: deque, index, element pointer; native pointers here). The elements are
// 4-byte words on the Mac, so the block is read as uint32_t.
struct MslCdequeIter {
    std::__cdeque<const void*, std::allocator<const void*>>* mDeque;  // +0x0
    uint32_t mIndex;                                                  // +0x4
    uint32_t* mPtr;                                                   // +0x8
};

namespace std {
template <>
struct __cdeque<const void*, allocator<const void*>> {
    uint32_t mWord0;  // 0x0
    uint32_t mWord4;  // 0x4
    uint32_t mWord8;  // 0x8
    uint32_t mWordC;  // 0xc
    void swap(__cdeque& other);
    // Unit v5149 (src/std___cdeque_erase.cpp): word0 capacity, word4 size, word8 start, wordC block of 4-byte elements.
    MslCdequeIter erase(MslCdequeIter first, MslCdequeIter last);  // 0x101f3704 (856); answers first
    void resize(uint32_t n, const void* value);                    // 0x101f2a10 (396)
    void push_back(const void* const& value);                      // 0x101f459c (828)
    void reserve(uint32_t n);                                      // 0x101f2730 (660), unit v5925
    void pop_front();                                              // 0x101f25e8 (52), unit v51076: size - 1, start + 1 wrapped at the capacity
    uint32_t max_size() const;                                     // 0x101f492c (36), unit v51076: 0x3fffffff
};
}  // namespace std

// Unit v5149: callees outside the unit (fakes in the tests).
namespace cdeque_callee {
void Insert(std::__cdeque<const void*, std::allocator<const void*>>* self, MslCdequeIter pos, uint32_t count,
            const void* const* value);                            // 0x101f3b10
MslCdequeIter* IterCopy(MslCdequeIter* dest, const MslCdequeIter* src);  // 0x101f2dd8
MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n);                    // 0x101f2bec
}  // namespace cdeque_callee
extern uint32_t gCdequeMinCap;  // 0x102b1420: the smallest capacity push_back grows to (initial word 1)
// Unit v5907: do_assign<__cdeque<vector<Argument>*>::iterator> 0x10229ae4 (836): the iterators (deque, index, element pointer) by value.
void CdequeDoAssignArgumentIter(std::__cdeque<const void*, std::allocator<const void*>>* self, MslCdequeIter first, MslCdequeIter last);
