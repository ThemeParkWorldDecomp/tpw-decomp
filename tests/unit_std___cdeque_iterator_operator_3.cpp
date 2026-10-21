// Test library for the CodeWarrior cdeque iterator operator-=, queue<errorthingy>::~queue and __cdeque<const void*>::reserve (unit v5925;
// libtpw_unit_std___cdeque_iterator_operator_3.dylib, tests/test_std___cdeque_iterator_operator_3.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are recording fakes: operator+= , tear_down, ~__cdeque, max_size and the runtime imports. Addresses
// are logged relative to a base the wrapper sets (the host's heap differs from the emulator's).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"

using namespace calllog;

// ---- globals of other files, defined here
uint64_t gMslMacAddressHigh = 0;
namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

uint64_t gBase = 0;      // the object the log is relative to
uint64_t gOldBlock = 0;  // the block table the reserve starts with

int64_t Rel(const void* p) { return p == nullptr ? -1 : static_cast<int64_t>(reinterpret_cast<uint64_t>(p) - gBase) & 0xffffffff; }

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s; s++) h = h * 31u + static_cast<uint8_t>(*s);
    return h;
}

void* NewBlock(uint32_t size) { return tpw::ArenaAllocZeroed(size < 4 ? 4 : size); }

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
namespace cdeque_callee {
MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n) {
    ev("IterAdd", {n});
    return it;
}
uint32_t MaxSize(const std::__cdeque<const void*, std::allocator<const void*>>*) {
    ev("MaxSize", {});
    return static_cast<uint32_t>(ret("MaxSize"));
}
}  // namespace cdeque_callee

namespace errdeque_callee {
void TearDown(MslErrorDeque* self) { ev("TearDown", {Rel(self)}); }
void QueueCdequeDtor(void* cdeque, int16_t flag) { ev("CdequeDtor", {Rel(cdeque), flag}); }
void OperatorDelete(void* p) { ev("Delete", {Rel(p)}); }
}  // namespace errdeque_callee

namespace IME {
size_t MslStrlen(const char* s) {
    ev("Strlen", {Hash(s)});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("Strcpy", {Hash(s)});
    return std::strcpy(d, s);
}
void* MslNew(uint32_t size) {
    ev("New", {size});
    return NewBlock(size);
}
void* MslNewArray(uint32_t size) {
    ev("NewArray", {size});
    return NewBlock(size);
}
void MslDelete(void* p) { ev("Delete", {reinterpret_cast<uint64_t>(p) == gOldBlock ? 1 : 0}); }
void MslDeleteArray(void*) { ev("DeleteArray", {}); }
void MslThrow(const char* type, void* object, const void* dtor) {
    auto* o = static_cast<MslExceptionObject*>(object);
    int64_t vtbl = o->mVtbl == gMslLengthErrorVtbl ? 1 : o->mVtbl == gMslExcVtblB ? 2 : o->mVtbl == gMslExcVtblC ? 3 : 0;
    ev("Throw", {Hash(type), vtbl, dtor == gMslLengthErrorDtorTV ? 1 : 0, Hash(o->mRc.mPtr), o->mRc.mCount ? *o->mRc.mCount : -1});
}
}  // namespace IME

extern "C" {

uint64_t tpw_cdi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cdi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(std::__cdeque<const void*, std::allocator<const void*>>));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mBlocks));
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDeque));
    return n;
}

// operator-=: the iterator (deque, index, pointer word) and the count; answers index and pointer word in out.
void tpw_cdi_sub(uint64_t deque, uint32_t index, uint32_t ptr, int32_t n, uint32_t* out) {
    gMslMacAddressHigh = deque >> 32;
    MslCdequeIter it;
    it.mDeque = reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(static_cast<uintptr_t>(deque));
    it.mIndex = index;
    it.mPtr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | ptr));
    gBase = 0;
    MslCdequeIterSub(&it, n);
    out[0] = it.mIndex;
    out[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(it.mPtr));
}

uint64_t tpw_cdi_queue(uint64_t self, int16_t flag) {
    gBase = self;
    return reinterpret_cast<uint64_t>(MslQueueErrorDestroy(reinterpret_cast<MslErrorDeque*>(static_cast<uintptr_t>(self)), flag));
}

void tpw_cdi_reserve(uint64_t deque, uint32_t n) {
    auto* d = reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(static_cast<uintptr_t>(deque));
    gMslMacAddressHigh = deque >> 32;
    gBase = 0;
    gOldBlock = d->mWordC == 0 ? ~0ull : ((gMslMacAddressHigh << 32) | d->mWordC);
    d->reserve(n);
}

}  // extern "C"
