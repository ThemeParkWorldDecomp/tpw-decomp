// Test library for std::basic_string<wchar_t> replace / substr / begin (unit v5044; libtpw_unit_std_basic_string_replace.dylib,
// tests/test_std_basic_string_replace.py), main app:
//   src/ui_iterators_std_basic_string_replace.cpp
//
// The callees outside the unit (strlen, strcpy, the allocators, __throw, the wmem functions, the counted-pointer constructor and the
// substring constructor) are recording fakes with the same events as the module's PowerPC stubs. The globals of other files are
// defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {

// ---- globals of other files, defined here
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
char16_t gMslWcharNulA;
char16_t gMslWcharNulB;

namespace {

uint32_t Hash(const char* s) {
    uint32_t h = 0x811C9DC5u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 0x01000193u;
    return h;
}

constexpr uint32_t kMaxOp = 4096;  // the fakes move memory only for counts up to this (a wrapped count would run wild)

void* Alloc(uint32_t n) { return tpw::ArenaAllocZeroed(n < 4 ? 4 : n); }

}  // namespace

// ---- the fakes
size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {static_cast<int64_t>(Hash(s)), static_cast<int64_t>(n)});
    return n;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {static_cast<int64_t>(Hash(s))});
    std::strcpy(d, s);
    return d;
}
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return Alloc(size);
}
void* MslNewRep() {
    ev("new", {0x10});
    return Alloc(sizeof(MslWideStringRep));
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {static_cast<int64_t>(size)});
    return Alloc(size);
}
void MslDelete(void* p) { ev("delete", {lab(p)}); }
void MslDeleteArray(void* p) { ev("delete[]", {lab(p)}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* e = static_cast<const MslExceptionObject*>(object);
    int64_t count = e->mRc.mCount ? static_cast<int64_t>(static_cast<uint32_t>(*e->mRc.mCount)) : 0xFFFFFFFFll;
    ev("throw", {static_cast<int64_t>(Hash(typeName)), lab(e->mVtbl), static_cast<int64_t>(Hash(e->mRc.mPtr)), count, lab(dtorTV)});
}
char16_t* MslWmemcpy(char16_t* d, const char16_t* s, uint32_t n) {
    ev("wmemcpy", {lab(d), lab(s), static_cast<int64_t>(n)});
    if (n <= kMaxOp) std::memmove(d, s, n * 2);
    return d;
}
char16_t* MslWmemmove(char16_t* d, const char16_t* s, uint32_t n) {
    ev("wmemmove", {lab(d), lab(s), static_cast<int64_t>(n)});
    if (n <= kMaxOp) std::memmove(d, s, n * 2);
    return d;
}
char16_t* MslWmemset(char16_t* d, char16_t c, uint32_t n) {
    ev("wmemset", {lab(d), static_cast<int64_t>(c), static_cast<int64_t>(n)});
    if (n <= kMaxOp)
        for (uint32_t i = 0; i < n; ++i) d[i] = c;
    return d;
}
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcp", {p != nullptr ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}
MslWideString* MslWideString_construct(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    ev("construct", {lab(self), lab(str), static_cast<int64_t>(pos), static_cast<int64_t>(n)});
    return self;
}

}  // namespace IME

extern "C" {

uint64_t tpw_sbr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"nulA", &IME::gMslWcharNulA},
        {"nulB", &IME::gMslWcharNulB},
        {"vtblA", IME::gMslExcVtblA},
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
        {"vtblLen", IME::gMslLengthErrorVtbl},
        {"tvRange", IME::gMslOutOfRangeDtorTV},
        {"tvLen", IME::gMslLengthErrorDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sbr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sbr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mLength));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mCapacity));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideStringRep));
    return n;
}

uint64_t tpw_sbr_replace(uint64_t self, uint32_t pos, uint32_t n1, uint32_t n2, uint32_t c) {
    IME::MslWideString_replace(reinterpret_cast<IME::MslWideString*>(self), pos, n1, n2, static_cast<char16_t>(c));
    return self;
}
uint64_t tpw_sbr_substr(uint64_t result, uint64_t self, uint32_t pos, uint32_t n) {
    return reinterpret_cast<uintptr_t>(IME::MslWideString_substr(reinterpret_cast<IME::MslWideString*>(result),
                                                                 reinterpret_cast<const IME::MslWideString*>(self), pos, n));
}
uint64_t tpw_sbr_begin(uint64_t self) {
    return reinterpret_cast<uintptr_t>(IME::MslWideString_begin(reinterpret_cast<IME::MslWideString*>(self)));
}

}  // extern "C"
