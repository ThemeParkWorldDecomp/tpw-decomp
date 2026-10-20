// Test library for std::vector<Argument> copy constructor and push_back (unit v5181; libtpw_unit_std_vector_vector.dylib,
// tests/test_std_vector_vector.py), main app:
//   src/std_vector_vector.cpp   ArgumentVector::ArgumentVector(const&) 0x10227200, ArgumentVector::push_back 0x10201478
//
// The callees outside the unit are recording fakes: operator new / new[] / delete, strlen, strcpy, __throw (returns), the counted pointer
// constructor, Argument's copy constructor (copies the three words) and std::__destroy<Argument*>. Strings are logged as (length, byte sum).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "ui_ime.h"
#include "online.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {
int64_t Len(const char* s) { return static_cast<int64_t>(std::strlen(s)); }
int64_t Sum(const char* s) {
    int64_t sum = 0;
    for (; *s; ++s) sum += static_cast<uint8_t>(*s);
    return sum;
}
}  // namespace

namespace IME {
size_t MslStrlen(const char* s) {
    ev("strlen", {Len(s), Sum(s)});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {Len(s), Sum(s)});
    return std::strcpy(d, s);
}
void* MslNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size);
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {size});
    return tpw::ArenaAllocZeroed(size);
}
void MslDelete(void* p) { ev("delete", {lab(p)}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* exc = static_cast<MslExceptionObject*>(object);
    ev("throw", {Len(typeName), Sum(typeName), lab(dtorTV), lab(exc->mVtbl), Len(exc->mRc.mPtr), Sum(exc->mRc.mPtr)});
}
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {lab(p)});
    self->mPtr = p;
}
}  // namespace IME

namespace std_vector_callee {
void ArgumentCopy(Argument* self, const Argument* other) {
    ev("copy", {other->mString, other->mWString, other->mCommand});
    std::memcpy(self, other, 12);
}
void ArgumentDestroyRange(Argument* first, Argument* last) {
    ev("destroy", {lab(first), (reinterpret_cast<const char*>(last) - reinterpret_cast<const char*>(first)) / 12});
}
}  // namespace std_vector_callee

extern "C" {

uint64_t tpw_svv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"lenVtbl", IME::gMslLengthErrorVtbl},
        {"dtorTV", IME::gMslLengthErrorDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_svv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_svv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mCapacity));
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mSize));
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mData));
    out[n++] = static_cast<int64_t>(sizeof(ArgumentVector));
    return n;
}

uint64_t tpw_svv_ctor(uint64_t self, uint64_t other) {
    ArgumentVector* v = new (reinterpret_cast<void*>(static_cast<uintptr_t>(self)))
        ArgumentVector(*reinterpret_cast<const ArgumentVector*>(static_cast<uintptr_t>(other)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(v));
}

void tpw_svv_push_back(uint64_t self, uint64_t argument) {
    reinterpret_cast<ArgumentVector*>(static_cast<uintptr_t>(self))->push_back(*reinterpret_cast<const Argument*>(static_cast<uintptr_t>(argument)));
}

}  // extern "C"
