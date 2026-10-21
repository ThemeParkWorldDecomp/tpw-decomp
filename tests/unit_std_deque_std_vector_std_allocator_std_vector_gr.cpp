// Test library for std::deque<std::vector<Argument>>::grow_buf_back, destroy and ~deque (unit v5976;
// libtpw_unit_std_deque_std_vector_std_allocator_std_vector_gr.dylib), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are fakes that record what the real ones read: operator new/delete, the block table's reserve, push_back
// and do_assign, tear_down, the iterator copy and +=, and __destroy. Pointers are logged as (region number, offset) of the blocks the
// test registered or the fake operator new made, so the two sides' different addresses compare equal.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "std_cdeque.h"

using namespace calllog;

uint64_t gMslMacAddressHigh = 0;

namespace {

struct Region {
    uint32_t base;
    uint32_t size;
};
Region gRegions[512];
int gRegionCount = 0;

struct Desc {
    int64_t id;
    int64_t off;
};

void* Alloc(uint32_t size) {
    void* p = tpw::ArenaAllocZeroed(size);
    gMslMacAddressHigh = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)) >> 32;
    if (gRegionCount < 512) gRegions[gRegionCount++] = {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)), size};
    return p;
}

Desc Describe(uint64_t address) {
    uint32_t w = static_cast<uint32_t>(address);
    for (int i = 0; i < gRegionCount; i++)
        if (w >= gRegions[i].base && w - gRegions[i].base < gRegions[i].size) return {i + 1, static_cast<int64_t>(w - gRegions[i].base)};
    return {0, static_cast<int64_t>(w)};
}

Desc D(const void* p) { return Describe(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p))); }

uint32_t* Native(uint32_t w) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | w));
}

}  // namespace

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return Alloc(size);
}
void OperatorDelete(void* p) {
    Desc d = D(p);
    ev("delete", {d.id, d.off});
}
void DequeTearDown(void* self) {
    Desc d = D(self);
    ev("tear_down", {d.id, d.off});
}
}  // namespace online_callee

namespace argdeque_callee {
void CdequeReserve(void* cdeque, uint32_t n) {
    uint32_t* w = static_cast<uint32_t*>(cdeque);
    Desc d = D(cdeque);
    Desc t = Describe(w[3]);
    ev("reserve", {d.id, d.off, static_cast<int64_t>(n), static_cast<int64_t>(w[0]), static_cast<int64_t>(w[1]), static_cast<int64_t>(w[2]), t.id, t.off});
}
// The fake adds one to the count while it is below the capacity (the real one grows the table).
void CdequePushBack(void* cdeque, const uint32_t* value) {
    uint32_t* w = static_cast<uint32_t*>(cdeque);
    Desc d = D(cdeque);
    Desc t = Describe(w[3]);
    ev("push_back", {d.id, d.off, static_cast<int64_t>(w[0]), static_cast<int64_t>(w[1]), static_cast<int64_t>(w[2]), t.id, t.off,
                     static_cast<int64_t>(*value)});
    if (w[1] < w[0]) w[1]++;
}
}  // namespace argdeque_callee

namespace cdeque_callee {
MslCdequeIter* IterCopy(MslCdequeIter* dest, const MslCdequeIter* src) {
    Desc q = D(src->mDeque);
    Desc p = D(src->mPtr);
    ev("iter_copy", {q.id, q.off, static_cast<int64_t>(src->mIndex), p.id, p.off});
    *dest = *src;
    return dest;
}
// The fake moves the index and the slot by n, wrapping once at the end of the table.
MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n) {
    Desc q = D(it->mDeque);
    Desc p = D(it->mPtr);
    ev("iter_add", {q.id, q.off, static_cast<int64_t>(it->mIndex), p.id, p.off, static_cast<int64_t>(n)});
    it->mIndex += n;
    it->mPtr += n;
    uint32_t* end = Native(it->mDeque->mWordC) + it->mDeque->mWord0;
    if (it->mPtr >= end) it->mPtr -= it->mDeque->mWord0;
    return it;
}
}  // namespace cdeque_callee

void CdequeDoAssignArgumentIter(std::__cdeque<const void*, std::allocator<const void*>>* self, MslCdequeIter first, MslCdequeIter last) {
    Desc s = D(self);
    Desc q1 = D(first.mDeque);
    Desc p1 = D(first.mPtr);
    Desc q2 = D(last.mDeque);
    Desc p2 = D(last.mPtr);
    ev("do_assign", {s.id, s.off, q1.id, q1.off, static_cast<int64_t>(first.mIndex), p1.id, p1.off, q2.id, q2.off,
                     static_cast<int64_t>(last.mIndex), p2.id, p2.off});
}

namespace argdestroy_callee {
void DestroyRange(uint32_t first, uint32_t last) {
    Desc d = Describe(first);
    ev("destroy", {d.id, d.off, static_cast<int64_t>(static_cast<uint32_t>(last - first))});
}
}  // namespace argdestroy_callee

extern "C" {

uint64_t tpw_argdq_addr(const char* name) {
    if (std::strcmp(name, "fill") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gArgDequeGrowFill));
    return 0;
}

// A zeroed registered block (32-bit addressable).
uint64_t tpw_argdq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Alloc(size))); }

void tpw_argdq_reset() { gRegionCount = 0; }
int32_t tpw_argdq_regcount() { return gRegionCount; }
uint64_t tpw_argdq_region(int32_t i) { return static_cast<uint64_t>(gRegions[i].base) | (static_cast<uint64_t>(gRegions[i].size) << 32); }

int32_t tpw_argdq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslArgumentDeque));
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentDeque, mTable));
    out[n++] = static_cast<int64_t>(sizeof(MslArgumentDequeIter));
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentDequeIter, mElement));
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentDequeIter, mIndex));
    return n;
}

uint32_t tpw_argdq_grow(uint64_t self, uint32_t from, uint64_t spare, uint64_t added, uint32_t to) {
    return argdeque_callee::GrowBufBack(reinterpret_cast<MslArgumentDeque*>(self), from, reinterpret_cast<void*>(spare),
                                        reinterpret_cast<uint32_t*>(added), to)
               ? 1u
               : 0u;
}

void tpw_argdq_destroy(uint64_t self, const uint32_t* a, const uint32_t* b) {
    MslArgumentDequeIter first, last;
    std::memcpy(&first, a, sizeof first);
    std::memcpy(&last, b, sizeof last);
    ArgumentDequeDestroy(reinterpret_cast<MslArgumentDeque*>(self), first, last);
}

uint32_t tpw_argdq_dtor(uint64_t self, int16_t flag) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ArgumentDequeDestruct(reinterpret_cast<MslArgumentDeque*>(self), flag)));
}

}  // extern "C"
