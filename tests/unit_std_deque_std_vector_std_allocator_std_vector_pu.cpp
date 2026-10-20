// Test library for the Metrowerks std::deque<std::vector<Argument>> (unit v5938; libtpw_unit_std_deque_std_vector_std_allocator_std_vector_pu.dylib,
// tests/test_std_deque_std_vector_std_allocator_std_vector_pu.py), main app:
//   src/std_deque_std_vector_std_allocator_std_vector_pu.cpp   push_back, pop_front, the copy constructor, make_iterator
//
// The callees outside the unit are fakes that record what the real ones read: the block table (a std::__cdeque<const void*>) is modelled
// by a few words of arithmetic, the vector copy constructor and destructor write a marker into the element, the runtime imports bump-allocate
// inside one pool the test hands over (so every address is recorded as an offset into the pool, the same on both sides).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

uint64_t gMslMacAddressHigh = 0;   // the arena is 32-bit addressable

namespace IME {
char gMslExcVtblB[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

uint64_t gPoolBase = 0;
uint32_t gPoolNext = 0;
uint32_t gSeq = 0;
const uint32_t kBumpStart = 0x2000;

uint32_t Bump(uint32_t size) {
    uint32_t at = gPoolNext;
    gPoolNext += (size + 15) & ~15u;
    return static_cast<uint32_t>(gPoolBase) + at;
}
void* Ptr(uint32_t w) { return reinterpret_cast<void*>(static_cast<uintptr_t>(gMslMacAddressHigh | w)); }
uint32_t W(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
// A word inside the pool as its offset, any other word (a null, a stray value) as it is.
int64_t Off(uint32_t w) {
    uint32_t d = w - static_cast<uint32_t>(gPoolBase);
    return d < 0x6000u ? static_cast<int64_t>(d) : static_cast<int64_t>(w);
}
int64_t OffP(const void* p) { return Off(W(p)); }
int64_t Sum(const char* s) {
    uint32_t sum = 0;
    for (uint32_t i = 0; s[i] != 0; i++) sum += static_cast<uint32_t>(static_cast<uint8_t>(s[i])) * (i + 1);
    return sum;
}
int64_t VtId(const void* p) {
    if (p == IME::gMslLengthErrorVtbl) return 1;
    if (p == IME::gMslExcVtblB) return 2;
    if (p == IME::gMslLengthErrorDtorTV) return 4;
    return 0;
}

struct Cd {
    uint32_t cap, count, start, table;
};

void CdReserve(Cd* c, uint32_t n) {
    if (n > c->cap) {
        uint32_t table = Bump(4 * n);
        uint32_t* fresh = static_cast<uint32_t*>(Ptr(table));
        const uint32_t* old = static_cast<const uint32_t*>(Ptr(c->table));
        for (uint32_t i = 0; i < c->count; i++) fresh[i] = old[(c->start + i) % c->cap];
        c->table = table;
        c->start = 0;
        c->cap = n;
    }
}

int64_t IterWord(const MslArgumentDequeIter* it, int i) {
    const uint32_t* w = reinterpret_cast<const uint32_t*>(it);
    if (i == 0 || i == 2 || i == 3 || i == 4) return Off(w[i]);
    return w[i];
}

}  // namespace

namespace argdeque_callee {
void CdequeReserve(void* cdeque, uint32_t n) {
    ev("cdeque_reserve", {OffP(cdeque), n});
    CdReserve(static_cast<Cd*>(cdeque), n);
}
void CdequeResize(void* cdeque, uint32_t n, const void* fill) {
    ev("cdeque_resize", {OffP(cdeque), n, W(fill)});
    Cd* c = static_cast<Cd*>(cdeque);
    uint32_t fillWord = fill != nullptr ? *static_cast<const uint32_t*>(fill) : 0;
    CdReserve(c, n);
    uint32_t* table = static_cast<uint32_t*>(Ptr(c->table));
    for (uint32_t i = c->count; i < n; i++) table[(c->start + i) % c->cap] = fillWord;
    c->count = n;
}
void CdequePushBack(void* cdeque, const uint32_t* value) {
    ev("cdeque_push_back", {OffP(cdeque), *value});
    Cd* c = static_cast<Cd*>(cdeque);
    if (c->count >= c->cap) CdReserve(c, c->count + 1);
    static_cast<uint32_t*>(Ptr(c->table))[(c->start + c->count) % c->cap] = *value;
    c->count++;
}
void CdequePopFront(void* cdeque) {
    ev("cdeque_pop_front", {OffP(cdeque)});
    Cd* c = static_cast<Cd*>(cdeque);
    if (c->count > 0 && c->cap > 0) {
        c->start = (c->start + 1) % c->cap;
        c->count--;
    }
}
void* CdequeIndex(void* cdeque, uint32_t index) {
    ev("cdeque_index", {OffP(cdeque), index});
    Cd* c = static_cast<Cd*>(cdeque);
    uint32_t at = c->cap != 0 ? (c->start + index) % c->cap : 0;
    return static_cast<uint32_t*>(Ptr(c->table)) + at;
}
bool GrowBufBack(MslArgumentDeque* self, uint32_t from, void* spare, uint32_t* added, uint32_t to) {
    (void)added;
    uint32_t* s = static_cast<uint32_t*>(spare);
    ev("grow_buf_back", {OffP(self), from, to, Off(s[0]), s[1], s[2], Off(s[3])});
    bool answer = (ret("grow") & 1) != 0;
    if (ret("grow_spare") != 0) s[3] = Bump(16);
    return answer;
}
void VectorCopy(void* target, const void* source) {
    ev("vector_copy", {OffP(target), OffP(source)});
    uint32_t* t = static_cast<uint32_t*>(target);
    t[0] = 0x5A5A0000u + gSeq++;
    t[1] = static_cast<uint32_t>(OffP(source));
    t[2] = static_cast<uint32_t>(OffP(target));
}
void VectorDtor(void* vec, int16_t flag) {
    ev("vector_dtor", {OffP(vec), static_cast<int32_t>(flag)});
    static_cast<uint32_t*>(vec)[0] = 0x6B6B0000u + gSeq++;
}
void CursorAdd(MslArgumentCursor* cursor, int32_t count) {
    ev("cursor_add", {Off(cursor->mCdeque), cursor->mIndex, Off(cursor->mSlot), static_cast<uint32_t>(count)});
    cursor->mIndex += static_cast<uint32_t>(count);
    cursor->mSlot += 4 * static_cast<uint32_t>(count);
}
void* IterDeref(const MslArgumentDequeIter* it) {
    ev("iter_deref", {IterWord(it, 0), IterWord(it, 1), IterWord(it, 2), IterWord(it, 3), IterWord(it, 4), IterWord(it, 5)});
    return Ptr(it->mElement);
}
void IterInc(MslArgumentDequeIter* it) {
    ev("iter_inc", {IterWord(it, 0), IterWord(it, 1), IterWord(it, 2), IterWord(it, 3), IterWord(it, 4), IterWord(it, 5)});
    it->mElement += 0xc;
    it->mIndex += 1;
}
bool IterNotEqual(const MslArgumentDequeIter* a, const MslArgumentDequeIter* b) {
    ev("iter_ne", {IterWord(a, 0), IterWord(a, 1), IterWord(a, 2), IterWord(a, 3), IterWord(a, 4), IterWord(a, 5), IterWord(b, 0),
                   IterWord(b, 1), IterWord(b, 2), IterWord(b, 3), IterWord(b, 4), IterWord(b, 5)});
    return a->mIndex != b->mIndex;
}
}  // namespace argdeque_callee

namespace IME {
size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {static_cast<int64_t>(n), Sum(s)});
    return n;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {OffP(d), Sum(s)});
    // the bytes are not written: byte order differs between the two memories; the source's checksum is in the event
    return d;
}
void* MslNew(uint32_t size) {
    ev("new", {size});
    return Ptr(Bump(size));
}
void* MslNewArray(uint32_t size) {
    ev("new_array", {size});
    return Ptr(Bump(size));
}
void MslDelete(void* p) { ev("delete", {OffP(p)}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {Sum(typeName), VtId(o->mVtbl), OffP(o->mRc.mPtr), OffP(o->mRc.mCount), VtId(dtorTV)});
}
}  // namespace IME

extern "C" {

uint64_t tpw_argdq_addr(const char* name) {
    if (std::strcmp(name, "bs") == 0) return reinterpret_cast<uintptr_t>(&gArgDequeBlockSize);
    if (std::strcmp(name, "fill") == 0) return reinterpret_cast<uintptr_t>(&gArgDequeFill);
    return 0;
}

uint64_t tpw_argdq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The pool of one case: the fakes bump-allocate from kBumpStart on.
void tpw_argdq_pool(uint64_t base) {
    gPoolBase = base;
    gMslMacAddressHigh = base & ~0xFFFFFFFFull;   // the arena lies above 4 GB: the upper half of every address
    gPoolNext = kBumpStart;
    gSeq = 0;
}

int32_t tpw_argdq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslArgumentDeque));
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentDeque, mFirst));
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentDeque, mSize));
    out[n++] = static_cast<int64_t>(sizeof(MslArgumentDequeIter));
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentDequeIter, mIndex));
    return n;
}

void tpw_argdq_push_back(uint64_t deque, uint64_t vec) {
    online_callee::ArgumentDequePushBack(reinterpret_cast<void*>(deque), reinterpret_cast<void*>(vec));
}
void tpw_argdq_pop_front(uint64_t deque) { online_callee::ArgumentDequePopFront(reinterpret_cast<void*>(deque)); }
void tpw_argdq_ctor(uint64_t self, uint64_t other) { online_callee::DequeCopy(reinterpret_cast<void*>(self), reinterpret_cast<void*>(other)); }
void tpw_argdq_make_iterator(uint64_t out, uint64_t deque, uint32_t index) {
    ArgumentDequeMakeIterator(reinterpret_cast<MslArgumentDequeIter*>(out), reinterpret_cast<const MslArgumentDeque*>(deque), index);
}

}  // extern "C"
