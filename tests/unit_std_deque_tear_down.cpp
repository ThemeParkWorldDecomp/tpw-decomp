// Test library for the CodeWarrior std::deque<errorthingy> tear_down, copy constructor and destroy (unit v5680;
// libtpw_unit_std_deque_tear_down.dylib, tests/test_std_deque_tear_down.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are fakes that record what the PowerPC stubs record. The block table fakes (reserve, resize, push_back,
// operator new) hand out the memory the test pre-allocated (tpw_edq_set), so both sides see the same world.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "UI_pump.h"
#include "std_deque_errorthingy.h"

using namespace calllog;

uint64_t gMslMacAddressHigh = 0;   // the arena is 32-bit addressable

namespace {

uint32_t gNewTab = 0;
uint32_t gNewBlk = 0;

uint32_t* W(uint32_t word) { return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }
uint32_t Lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
int64_t Lab(uint32_t word) { return lab(W(word)); }

// (label of the block table, block number, slot as an index into the table)
void Cur(const MslErrorDequeCursor* c, int64_t* out) {
    uint32_t table = W(c->mCdeque)[3];
    out[0] = Lab(c->mCdeque);
    out[1] = c->mWord4;
    out[2] = static_cast<uint32_t>(c->mSlot - table) / 4;
}

void MakeIt(const char* name, MslErrorDequeIterator* out, const MslErrorDeque* self, uint32_t index) {
    ev(name, {Lab(Lo(self)), static_cast<int64_t>(index)});
    uint32_t blockSize = self->mBlockSize;
    uint32_t cd = Lo(&self->mBlocks);
    const uint32_t* c = W(cd);
    uint32_t b = index / blockSize;
    uint32_t slot = c[3] + ((c[2] + b) % c[0]) * 4;
    uint32_t blk = *W(slot);
    out->mCursor.mCdeque = cd;
    out->mCursor.mWord4 = b;
    out->mCursor.mSlot = slot;
    out->mElement = blk + (index % blockSize) * 8;
    out->mBlockEnd = blk + blockSize * 8;
    out->mIndex = index;
}

}  // namespace

void MslErrorDeque::make_iterator(MslErrorDequeIterator* out, uint32_t index) const { MakeIt("make_iterator", out, this, index); }

MslErrorDequeCursor* MslErrorDequeCursorAddErrorthingy(MslErrorDequeCursor* cursor, int32_t count) {
    int64_t k[3];
    Cur(cursor, k);
    ev("CursorAdd", {k[0], k[1], k[2], static_cast<int64_t>(static_cast<uint32_t>(count))});
    const uint32_t* c = W(cursor->mCdeque);
    cursor->mWord4 += static_cast<uint32_t>(count);
    cursor->mSlot = c[3] + ((c[2] + cursor->mWord4) % c[0]) * 4;
    return cursor;
}

namespace errdeque_callee {

void MakeIteratorMutable(MslErrorDequeIterator* out, MslErrorDeque* self, uint32_t index) { MakeIt("make_iterator_nc", out, self, index); }

void CdequeDecrement(MslErrorDequeCursor* cursor) {
    int64_t k[3];
    Cur(cursor, k);
    ev("CursorDec", {k[0], k[1], k[2]});
    const uint32_t* c = W(cursor->mCdeque);
    uint32_t slot = cursor->mSlot;
    if (slot == c[3]) slot = c[3] + c[0] * 4;
    cursor->mSlot = slot - 4;
    cursor->mWord4 -= 1;
}

MslErrorDequeCursor* CursorCopy(MslErrorDequeCursor* dst, const MslErrorDequeCursor* src) {
    int64_t k[3];
    Cur(src, k);
    ev("CursorCopy", {k[0], k[1], k[2]});
    *dst = *src;
    return dst;
}

void CdequeReserve(void* cdeque, uint32_t n) {
    ev("reserve", {lab(cdeque), static_cast<int64_t>(n)});
    uint32_t* c = static_cast<uint32_t*>(cdeque);
    c[0] = n;
    c[3] = gNewTab;
    c[2] = static_cast<uint32_t>(ret("first")) % n;
}

void CdequeResize(void* cdeque, uint32_t n, const void* fill) {
    ev("resize", {lab(cdeque), static_cast<int64_t>(n), static_cast<int64_t>(Lo(fill))});
    uint32_t* c = static_cast<uint32_t*>(cdeque);
    c[1] = n;
    for (uint32_t i = 0; i < n; i++) W(c[3])[(c[2] + i) % c[0]] = Lo(fill);
}

void CdequePushBack(void* cdeque, const uint32_t* value) {
    ev("push_back", {lab(cdeque), lab(value), static_cast<int64_t>(*value)});
    uint32_t* c = static_cast<uint32_t*>(cdeque);
    if (c[1] < c[0]) {
        W(c[3])[(c[2] + c[1]) % c[0]] = *value;
        c[1]++;
    }
}

void OperatorDelete(void* p) { ev("delete", {lab(p)}); }

void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return W(gNewBlk);
}

}  // namespace errdeque_callee

extern "C" {

uint64_t tpw_edq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"BS", &gMslErrorDequeBlockSize},
        {"FILL", &gMslErrorDequeFill},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The cursor holds 32-bit words: the upper half of the arena's addresses is gMslMacAddressHigh.
uint64_t tpw_edq_alloc(uint32_t size) {
    uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size)));
    gMslMacAddressHigh = address >> 32;
    return address;
}

void tpw_edq_set(uint32_t newTab, uint32_t newBlk) {
    gNewTab = newTab;
    gNewBlk = newBlk;
}

int32_t tpw_edq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mBlocks));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mFirst));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mSize));
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDeque));
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDequeIterator));
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDequeCursor));
    return n;
}

void tpw_edq_tear(uint32_t self) { errdeque_callee::TearDown(reinterpret_cast<MslErrorDeque*>(W(self))); }

void tpw_edq_copy(uint32_t self, uint32_t other) { errsinit_callee::DequeCopy(W(self), W(other)); }

void tpw_edq_destroy(uint32_t self, const uint32_t* first, const uint32_t* last) {
    MslErrorDequeIterator a;
    MslErrorDequeIterator b;
    std::memcpy(&a, first, sizeof a);
    std::memcpy(&b, last, sizeof b);
    MslErrorDeque_destroy(reinterpret_cast<MslErrorDeque*>(W(self)), &a, &b);
}

}  // extern "C"
