// Test library for std::__cdeque<const void*>::insert (unit v51030; libtpw_unit_std___cdeque_insert_2.dylib, tests/test_std___cdeque_insert_2.py), main app:
//   src/std___cdeque_insert_2.cpp   cdeque_callee::Insert; src/std___cdeque_iterator_operator_2.cpp supplies IterAdd (the original runs on the PowerPC side).
//
// The callees outside the unit (strlen, operator new[], operator new, operator delete, strcpy, __throw, the iterator copy) are fakes that
// record what the real one reads. __throw does not return on the Mac: the fake jumps out of the call, the PowerPC stub jumps to the epilogue.
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_cdeque.h"
#include "ui_ime.h"

using namespace calllog;

// ---- globals of other files, defined here
uint32_t gCdequeMinCap;
namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

using Deque = std::__cdeque<const void*, std::allocator<const void*>>;

Deque* gDeque = nullptr;
std::jmp_buf gJump;

int64_t Hash(const char* text) {
    uint32_t h = 0;
    for (const char* p = text; *p != 0; p++) h = h * 31 + static_cast<unsigned char>(*p);
    return static_cast<int64_t>(h & 0x7fffffffu);
}

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    ev("strlen", {static_cast<int64_t>(std::strlen(s)), Hash(s)});
    return std::strlen(s);
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {static_cast<int64_t>(std::strlen(s)), Hash(s)});
    return std::strcpy(d, s);
}

void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}

void* MslNewArray(uint32_t size) {
    ev("new_array", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}

void MslDelete(void* p) {
    bool isOld = p == reinterpret_cast<void*>(tpw::ArenaBase() + gDeque->mWordC);
    ev("delete", {isOld ? 1 : 0});
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* obj = static_cast<MslExceptionObject*>(object);
    ev("throw", {obj->mVtbl == gMslLengthErrorVtbl ? 1 : 0, dtorTV == gMslLengthErrorDtorTV ? 1 : 0,
                 obj->mRc.mCount != nullptr ? static_cast<int64_t>(*obj->mRc.mCount) : -1, static_cast<int64_t>(std::strlen(obj->mRc.mPtr)),
                 Hash(obj->mRc.mPtr), static_cast<int64_t>(std::strlen(typeName)), Hash(typeName)});
    std::longjmp(gJump, 1);
}

}  // namespace IME

namespace cdeque_callee {
MslCdequeIter* IterCopy(MslCdequeIter* dest, const MslCdequeIter* src) {
    *dest = *src;
    return dest;
}
}  // namespace cdeque_callee

extern "C" {

uint64_t tpw_cdi_addr(const char* name) {
    if (std::strcmp(name, "mincap") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gCdequeMinCap));
    if (std::strcmp(name, "arena") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase()));
    return 0;
}

uint64_t tpw_cdi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cdi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Deque));
    out[n++] = static_cast<int64_t>(offsetof(Deque, mWordC));
    return n;
}

void tpw_cdi_insert(uint64_t deque, uint64_t itDeque, uint32_t index, uint64_t itPtr, uint32_t count, uint64_t value) {
    gDeque = reinterpret_cast<Deque*>(static_cast<uintptr_t>(deque));
    MslCdequeIter pos{reinterpret_cast<Deque*>(static_cast<uintptr_t>(itDeque)), index, reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(itPtr))};
    if (setjmp(gJump) == 0) cdeque_callee::Insert(gDeque, pos, count, reinterpret_cast<const void* const*>(static_cast<uintptr_t>(value)));
}

}  // extern "C"
