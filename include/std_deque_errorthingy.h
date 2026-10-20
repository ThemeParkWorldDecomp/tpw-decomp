#pragma once
#include <cstdint>

#include "std_cdeque.h"

// Unit v5115: the CodeWarrior std::deque<errorthingy> (0x1c bytes on the Mac). The host's std::deque stands in for the queue
// elsewhere (UI_pump.h); this is the Mac's own layout, a block table of 8-byte records. All words are 32-bit Mac words.
//
// The deque: mBlockSize records per block, the block table (a __cdeque of block pointers: capacity, count, first slot, table pointer),
// the index of the first record and the number of records.
struct MslErrorDequeCursor {   // std::__cdeque<const void*>::const_iterator (0xc bytes)
    uint32_t mCdeque;   // 0x0 the block table (deque + 4)
    uint32_t mWord4;    // 0x4 the block number inside the table
    uint32_t mSlot;     // 0x8 address of the table slot holding the block pointer
};

struct MslErrorDequeIterator {   // std::deque<errorthingy>::iterator (0x18 bytes)
    MslErrorDequeCursor mCursor;   // 0x0
    uint32_t mElement;             // 0xc address of the record
    uint32_t mBlockEnd;            // 0x10 address past the record's block
    uint32_t mIndex;               // 0x14 index in the deque
};

struct MslErrorDeque {
    uint32_t mBlockSize;                                     // 0x0
    std::__cdeque<const void*, std::allocator<const void*>> mBlocks;   // 0x4 (word0 capacity +4, word4 count +8, word8 first slot +0xc, wordC table +0x10)
    uint32_t mFirst;                                         // 0x14
    uint32_t mSize;                                          // 0x18

    void pop_front();                                        // 0x101f2074
    void make_iterator(MslErrorDequeIterator* out, uint32_t index) const;   // 0x101f53b4 (the Mac returns it through r3)
};

// 0x101f52e0: the destructor with its hidden delete flag; answers `self`.
MslErrorDeque* MslErrorDequeDestroy(MslErrorDeque* self, int16_t deleteFlag);

// The callees outside the unit (fakes in the tests).
namespace errdeque_callee {
void* CdequeIndex(void* cdeque, uint32_t index);              // 0x101f2004 std::__cdeque<const void*>::operator[]
void TearDown(MslErrorDeque* self);                           // 0x101f4a74 std::deque<errorthingy>::tear_down
void CursorAdd(MslErrorDequeCursor* cursor, int32_t count);   // 0x101f5508 const_iterator::operator+=
void OperatorDelete(void* p);                                 // 0x10238218
}  // namespace errdeque_callee

// Unit v5135: std::__cdeque<errorthingy*>::iterator::operator+= 0x101f5adc (answers the cursor); a negative count goes to
// std::__cdeque<const void*>::iterator::operator-= 0x101f5bdc (fake in the tests).
MslErrorDequeCursor* MslErrorDequeCursorAddErrorthingy(MslErrorDequeCursor* cursor, int32_t count);
namespace errdeque_callee {
void CursorSub(MslErrorDequeCursor* cursor, int32_t count);   // 0x101f5bdc
}
// The host's heap lies above 4 GB while the cursor holds 32-bit words: the upper half of the address of a block table is this
// value (0 on the Mac's flat 32-bit space). The tests set it to the upper half of their allocations.
extern uint64_t gMslMacAddressHigh;

// Unit v5243: std::deque<errorthingy>::push_back 0x101f21ac and grow_buf_back 0x101f2e7c (src/UI_pump_std_deque_push_back.cpp).
// The record is the Mac's two words (a table pointer and an id). The iterator handed to the block table's do_assign is three words on
// the Mac (cdeque, index, slot); native pointers here.
struct MslErrorDequeAssignIter {
    const void* mCdeque;   // 0x0
    uint32_t mIndex;       // 0x4
    uint32_t* mSlot;       // 0x8 address of a table slot
};
extern uint32_t gMslErrorDequePad;   // 0x102b141c: the word push_back of the block table appends (zero at start)
void MslErrorDeque_push_back(MslErrorDeque* self, const uint32_t* record);   // 0x101f21ac
// 0x101f2e7c: answers 1 when it swapped the block table with `spare` (the Mac's r3 is that flag)
uint32_t MslErrorDeque_grow_buf_back(MslErrorDeque* self, uint32_t from, std::__cdeque<const void*, std::allocator<const void*>>* spare,
                                     uint32_t* added, uint32_t to);
namespace errdeque_callee {
void CdequeReserve(void* cdeque, uint32_t n);                                  // 0x101f2730
void CdequeResize(void* cdeque, uint32_t n, const void* fill);                 // 0x101f2a10
void CdequePopFront(void* cdeque);                                             // 0x101f25e8
void CdequePushBack(void* cdeque, const uint32_t* value);                      // 0x101f459c
void CdequeDoAssign(void* cdeque, const MslErrorDequeAssignIter& first, const MslErrorDequeAssignIter& last);   // 0x101f3278
}

// Unit v5680: tear_down 0x101f4a74 (errdeque_callee::TearDown), the copy constructor 0x101f4d14 (errsinit_callee::DequeCopy) and
// destroy 0x101f584c (src/sinit_ui_ErrorDisplay_cpp_std_deque_tear_down.cpp). destroy takes two iterators by value on the Mac
// (24 bytes each: r4..r9 and r10 + the stack); const pointers here.
extern uint32_t gMslErrorDequeBlockSize;   // 0x102b142c (0x40): records per block of a new deque
extern uint32_t gMslErrorDequeFill;        // 0x102b1430 (0): the word whose address is passed to the block table's push_back
void MslErrorDeque_destroy(MslErrorDeque* self, const MslErrorDequeIterator* first, const MslErrorDequeIterator* last);   // 0x101f584c
namespace errdeque_callee {
void MakeIteratorMutable(MslErrorDequeIterator* out, MslErrorDeque* self, uint32_t index);   // 0x101f56f8 make_iterator(ulong)
void CdequeDecrement(MslErrorDequeCursor* cursor);                                           // 0x101f4c84 iterator::operator--
MslErrorDequeCursor* CursorCopy(MslErrorDequeCursor* dst, const MslErrorDequeCursor* src);   // 0x101f5d6c iterator copy; answers dst
void* OperatorNew(uint32_t size);                                                            // 0x102384b8
}

// Unit v5925: iterator::operator-= 0x101f5bdc (answers nothing; a count of INT_MIN goes to cdeque_callee::IterAdd), queue::~queue
// 0x101f5200 (the queue is its deque at 0; hidden delete flag; answers self) and __cdeque<const void*>::reserve (std_cdeque.h).
void MslCdequeIterSub(MslCdequeIter* it, int32_t n);                   // 0x101f5bdc
MslErrorDeque* MslQueueErrorDestroy(MslErrorDeque* self, int16_t deleteFlag);   // 0x101f5200
namespace errdeque_callee {
void QueueCdequeDtor(void* cdeque, int16_t flag);   // 0x101f2520 std::__cdeque<errorthingy*>::~__cdeque (at queue + 4)
}
namespace cdeque_callee {
uint32_t MaxSize(const std::__cdeque<const void*, std::allocator<const void*>>* self);   // 0x101f492c
}
