// Test library for the CodeWarrior std::deque<errorthingy> (unit v5115; libtpw_unit_std_deque_pop_front.dylib, tests/test_std_deque_pop_front.py), main app:
//   src/game/MslSupport.cpp   pop_front, ~deque, make_iterator
//
// The callees outside the unit (the block table's operator[], tear_down, the cursor's +=, operator delete) are fakes recording what the
// real ones read.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_deque_errorthingy.h"

using namespace calllog;

namespace {
uint32_t gKnownObj, gKnownTab;   // the case's deque and block table: operator delete names them 1 and 3, anything else by its value
}

namespace errdeque_callee {

void* CdequeIndex(void* cdeque, uint32_t index) {
    ev("Index", {lab(cdeque), index});
    return nullptr;
}

void TearDown(MslErrorDeque* self) { ev("TearDown", {lab(self)}); }

// Moves the cursor on by `count` blocks: the block number and the slot address (4 bytes a block).
void CursorAdd(MslErrorDequeCursor* cursor, int32_t count) {
    ev("CursorAdd", {lab(static_cast<void*>(tpw::gArenaBase + cursor->mCdeque)), cursor->mWord4, count});
    cursor->mWord4 += static_cast<uint32_t>(count);
    cursor->mSlot += static_cast<uint32_t>(count) * 4;
}

void OperatorDelete(void* p) {
    // the pointer as the Mac's word: the offset from the arena base, 0 for null
    const uint32_t v = p == nullptr ? 0 : static_cast<uint32_t>(static_cast<const uint8_t*>(p) - tpw::gArenaBase);
    int64_t l = v == gKnownObj ? 1 : v == gKnownTab ? 3 : static_cast<int64_t>(v);
    ev("delete", {l});
}

}  // namespace errdeque_callee

extern "C" {

void tpw_edq_known(uint64_t obj, uint64_t tab) {
    gKnownObj = static_cast<uint32_t>(obj);   // Mac words (offsets from the arena base)
    gKnownTab = static_cast<uint32_t>(tab);
}

uint64_t tpw_edq_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

uint64_t tpw_edq_addr(const char*) { return 0; }

uint64_t tpw_edq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_edq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDeque));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mBlocks));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mFirst));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mSize));
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDequeIterator));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDequeIterator, mElement));
    return n;
}

void tpw_edq_pop(uint64_t self) { reinterpret_cast<MslErrorDeque*>(static_cast<uintptr_t>(self))->pop_front(); }

uint64_t tpw_edq_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(MslErrorDequeDestroy(reinterpret_cast<MslErrorDeque*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag))));
}

void tpw_edq_mk(uint64_t out, uint64_t self, uint32_t index) {
    reinterpret_cast<const MslErrorDeque*>(static_cast<uintptr_t>(self))->make_iterator(reinterpret_cast<MslErrorDequeIterator*>(static_cast<uintptr_t>(out)), index);
}

}  // extern "C"
