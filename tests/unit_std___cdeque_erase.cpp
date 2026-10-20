// Test library for the const void* deque (unit v5149; libtpw_unit_std___cdeque_erase.dylib, tests/test_std___cdeque_erase.py), main app:
//   src/std___cdeque_erase.cpp   erase, resize, push_back
//
// The callees outside the unit are fakes that record what the real ones read: insert, the iterator copy and +=, strlen, strcpy,
// operator new / new[] / delete and __throw. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "std_cdeque.h"
#include "ui_ime.h"

using namespace calllog;

uint32_t gCdequeMinCap = 1;  // 0x102b1420

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {
using Cdeque = std::__cdeque<const void*, std::allocator<const void*>>;
uintptr_t gNote = 0;  // the block the case calls the old one (delete logs whether it is freed)

int64_t I(uint64_t v) { return static_cast<int64_t>(v & 0xffffffffu); }

uint32_t Fnv(const char* s) {
    uint32_t h = 0x811c9dc5u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 0x01000193u;
    return h;
}

uint32_t* BufOf(const Cdeque* d) {
    return d->mWordC != 0 ? reinterpret_cast<uint32_t*>(tpw::gArenaBase + d->mWordC) : nullptr;
}
uint32_t Off(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
int64_t PtrOff(const MslCdequeIter* it) { return I(static_cast<uint64_t>(it->mPtr - BufOf(it->mDeque))); }
}  // namespace

namespace cdeque_callee {
void Insert(Cdeque* self, MslCdequeIter pos, uint32_t count, const void* const* value) {
    ev("Insert", {I(self->mWord0), I(self->mWord4), I(self->mWord8), pos.mDeque == self ? 1 : 0, I(pos.mIndex), PtrOff(&pos),
                  I(count), I(Off(*value))});
}

MslCdequeIter* IterCopy(MslCdequeIter* dest, const MslCdequeIter* src) {
    ev("IterCopy", {I(src->mIndex), PtrOff(src)});
    *dest = *src;
    return dest;
}

MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n) {
    ev("IterAdd", {I(it->mIndex), PtrOff(it), I(static_cast<uint32_t>(n))});
    uint32_t off = static_cast<uint32_t>(it->mPtr - BufOf(it->mDeque)) + static_cast<uint32_t>(n);
    if (off >= it->mDeque->mWord0) off -= it->mDeque->mWord0;
    it->mIndex += static_cast<uint32_t>(n);
    it->mPtr = BufOf(it->mDeque) + off;
    return it;
}
}  // namespace cdeque_callee

namespace IME {
size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {I(n), I(Fnv(s))});
    return n;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {I(std::strlen(s)), I(Fnv(s))});
    return std::strcpy(d, s);
}
void* MslNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size ? size : 4);
}
void* MslNewArray(uint32_t size) {
    ev("newarr", {I(size)});
    return tpw::ArenaAllocZeroed(size ? size : 4);
}
void MslDelete(void* p) { ev("delete", {reinterpret_cast<uintptr_t>(p) == gNote ? 1 : 0}); }
void MslDeleteArray(void* p) { ev("deletearr", {reinterpret_cast<uintptr_t>(p) == gNote ? 1 : 0}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* exc = static_cast<MslExceptionObject*>(object);
    int64_t vtbl = exc->mVtbl == gMslLengthErrorVtbl ? 1 : exc->mVtbl == gMslExcVtblB ? 2 : exc->mVtbl == gMslExcVtblC ? 3 : 0;
    int64_t count = exc->mRc.mCount ? I(static_cast<uint32_t>(*exc->mRc.mCount)) : -1;
    ev("throw", {I(Fnv(typeName)), vtbl, I(Fnv(exc->mRc.mPtr)), count, dtorTV == gMslLengthErrorDtorTV ? 1 : 0});
}
}  // namespace IME

extern "C" {

uint64_t tpw_cdq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gmin", &gCdequeMinCap},
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
        {"vtblLen", IME::gMslLengthErrorVtbl},
        {"dtorTV", IME::gMslLengthErrorDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cdq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_cdq_note(uint64_t p) { gNote = static_cast<uintptr_t>(p); }

int32_t tpw_cdq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Cdeque, mWord0));
    out[n++] = static_cast<int64_t>(offsetof(Cdeque, mWord4));
    out[n++] = static_cast<int64_t>(offsetof(Cdeque, mWord8));
    out[n++] = static_cast<int64_t>(offsetof(Cdeque, mWordC));
    out[n++] = static_cast<int64_t>(sizeof(Cdeque));
    return n;
}

// erase: the answer (deque, index, pointer) goes to out as three words (the pointers as host addresses)
void tpw_cdq_erase(uint64_t self, uint64_t fd, uint32_t fi, uint64_t fp, uint64_t ld, uint32_t li, uint64_t lp, uint64_t out) {
    MslCdequeIter first{reinterpret_cast<Cdeque*>(fd), fi, reinterpret_cast<uint32_t*>(fp)};
    MslCdequeIter last{reinterpret_cast<Cdeque*>(ld), li, reinterpret_cast<uint32_t*>(lp)};
    MslCdequeIter r = reinterpret_cast<Cdeque*>(self)->erase(first, last);
    uint32_t* o = reinterpret_cast<uint32_t*>(out);
    o[0] = Off(r.mDeque);
    o[1] = r.mIndex;
    o[2] = Off(r.mPtr);
}

uint64_t tpw_cdq_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

static const void* Val(uint32_t value) { return value != 0 ? static_cast<const void*>(tpw::gArenaBase + value) : nullptr; }

void tpw_cdq_resize(uint64_t self, uint32_t n, uint32_t value) { reinterpret_cast<Cdeque*>(self)->resize(n, Val(value)); }

void tpw_cdq_push_back(uint64_t self, uint32_t value) {
    const void* v = Val(value);
    reinterpret_cast<Cdeque*>(self)->push_back(v);
}

}  // extern "C"
