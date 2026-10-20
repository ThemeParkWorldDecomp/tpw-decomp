// Test library for std::__cdeque<const void*>::do_assign (unit v5564; libtpw_unit_std___cdeque_do_assign_std___cdeque_iterator.dylib), main app:
//   src/std___cdeque_do_assign_std___cdeque_iterator.cpp   CdequeDoAssign
//
// The runtime callees (strlen, strcpy, operator new, new[], delete, delete[], __throw) are recording fakes. The throw fake leaves the
// function by a C++ exception (the original never returns from __throw); the PowerPC stub jumps to the epilogue.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

uint64_t gMslMacAddressHigh = 0;

namespace {
uint32_t gOldBlock = 0;
struct HostThrow {};
}  // namespace

namespace IME {
size_t MslStrlen(const char* s) {
    ev("strlen", {}, {s});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {}, {s});
    std::strcpy(d, s);
    return d;
}
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
}
void MslDelete(void* p) {
    ev("delete", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)) == gOldBlock ? 1 : 0)});
}
void MslDeleteArray(void* p) { ev("delete[]", {lab(p)}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {lab(o->mVtbl), static_cast<int64_t>(*o->mRc.mCount), lab(dtorTV)}, {typeName, o->mRc.mPtr});
    throw HostThrow{};
}
}  // namespace IME

extern "C" {

uint64_t tpw_cdassign_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtbl", IME::gMslLengthErrorVtbl},
        {"dtor", IME::gMslLengthErrorDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cdassign_alloc(uint32_t size) {
    uint64_t a = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size)));
    gMslMacAddressHigh = a >> 32;
    return a;
}

int32_t tpw_cdassign_layout(int64_t* out) {
    int32_t n = 0;
    using Cd = std::__cdeque<const void*, std::allocator<const void*>>;
    out[n++] = static_cast<int64_t>(offsetof(Cd, mWordC));
    out[n++] = static_cast<int64_t>(sizeof(Cd));
    return n;
}

// Returns 1 when the call left through __throw.
uint32_t tpw_cdassign_do(uint32_t self, uint32_t cd1, uint32_t i1, uint32_t s1, uint32_t cd2, uint32_t i2, uint32_t s2) {
    auto full = [](uint32_t w) { return static_cast<uintptr_t>((gMslMacAddressHigh << 32) | w); };
    auto* cdeque = reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(full(self));
    gOldBlock = cdeque->mWordC;
    MslErrorDequeAssignIter first{reinterpret_cast<const void*>(full(cd1)), i1, reinterpret_cast<uint32_t*>(full(s1))};
    MslErrorDequeAssignIter last{reinterpret_cast<const void*>(full(cd2)), i2, reinterpret_cast<uint32_t*>(full(s2))};
    try {
        errdeque_callee::CdequeDoAssign(cdeque, first, last);
    } catch (const HostThrow&) {
        return 1;
    }
    return 0;
}

}  // extern "C"
