// Test library for std::basic_string<wchar_t> compare / assign / substring constructor (unit v5008;
// libtpw_unit_std_basic_string_compare.dylib, tests/test_std_basic_string_compare.py), main app:
//   src/game/MslSupport.cpp   IME::MslWideString_compare, _assign, _construct
//
// The callees outside the unit are fakes that record the arguments the real ones read (strings and character runs as FNV hashes,
// the same on the PowerPC side) and answer from the test's queue. __throw returns, like the PowerPC fake.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"

using namespace calllog;

namespace {

int64_t I(uint64_t v) { return static_cast<int64_t>(v); }

uint32_t FnvBytes(const char* s) {
    uint32_t h = 2166136261u;
    if (s == nullptr) return h;
    for (; *s; ++s) h = (h ^ static_cast<unsigned char>(*s)) * 16777619u;
    return h;
}

uint32_t Fnv16(const char16_t* p, uint32_t n) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

uint32_t Min(uint32_t a, uint32_t b) { return a < b ? a : b; }

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {I(FnvBytes(s)), I(n)});
    return n;
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {lab(d), I(FnvBytes(s))});
    std::strcpy(d, s);
    return d;
}

void* MslNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size < 32 ? 32 : size);
}

void* MslNewArray(uint32_t size) {
    ev("newa", {I(size)});
    return tpw::ArenaAllocZeroed(size < 32 ? 32 : size);
}

void MslDelete(void* p) { ev("delete", {lab(p)}); }

void MslDeleteArray(void* p) { ev("deletea", {lab(p)}); }

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* ex = static_cast<MslExceptionObject*>(object);
    ev("throw", {I(FnvBytes(typeName)), lab(const_cast<void*>(ex->mVtbl)), I(FnvBytes(ex->mRc.mPtr)), lab(const_cast<void*>(dtorTV))});
}

int32_t MslWmemcmp(const char16_t* a, const char16_t* b, uint32_t n) {
    uint32_t k = Min(n, 16);
    ev("wmemcmp", {I(n), I(Fnv16(a, k)), I(Fnv16(b, k))});
    return static_cast<int32_t>(ret("wmemcmp"));
}

char16_t* MslWmemcpy(char16_t* d, const char16_t* s, uint32_t n) {
    ev("wmemcpy", {I(n), lab(d), I(Fnv16(s, Min(n, 8)))});
    if (n) std::memmove(d, s, static_cast<size_t>(n) * 2);
    return d;
}

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcptr", {p != nullptr ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}

void MslWideString_changeSize(MslWideString* self, uint32_t n, bool keep) { ev("change_size", {lab(self), I(n), keep ? 1 : 0}); }

void MslWideString_replace(MslWideString* self, uint32_t pos, uint32_t n1, uint32_t n2, char16_t c) {
    ev("replace", {lab(self), I(pos), I(n1), I(n2), I(c)});
}

void MslWideString_init(MslWideString* self, const char16_t* first, const char16_t* last) {
    uint32_t n = static_cast<uint32_t>(last - first);
    int64_t chars[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (uint32_t i = 0; i < 8 && i < n; ++i) chars[i] = first[i];
    ev("init", {lab(self), I(n), chars[0], chars[1], chars[2], chars[3], chars[4], chars[5], chars[6], chars[7]});
}

}  // namespace IME

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sbs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"excA", IME::gMslExcVtblA},
        {"excB", IME::gMslExcVtblB},
        {"excC", IME::gMslExcVtblC},
        {"tv", IME::gMslOutOfRangeDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable).
uint64_t tpw_sbs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Offsets the module relies on: the representation's fields, the size of the string object.
int32_t tpw_sbs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mLength));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mCapacity));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideString));
    return n;
}

int32_t tpw_sbs_compare(uint64_t self, uint32_t pos, uint32_t n1, uint64_t s, uint32_t n2) {
    return IME::MslWideString_compare(reinterpret_cast<IME::MslWideString*>(self), pos, n1, reinterpret_cast<const char16_t*>(s), n2);
}

uint64_t tpw_sbs_assign(uint64_t self, uint64_t str, uint32_t pos, uint32_t n) {
    return reinterpret_cast<uintptr_t>(IME::MslWideString_assign(reinterpret_cast<IME::MslWideString*>(self),
                                                                  reinterpret_cast<IME::MslWideString*>(str), pos, n));
}

uint64_t tpw_sbs_construct(uint64_t self, uint64_t str, uint32_t pos, uint32_t n) {
    return reinterpret_cast<uintptr_t>(IME::MslWideString_construct(reinterpret_cast<IME::MslWideString*>(self),
                                                                     reinterpret_cast<IME::MslWideString*>(str), pos, n));
}

}  // extern "C"
