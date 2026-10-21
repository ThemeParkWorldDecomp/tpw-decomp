// Test library for the MSL std::basic_string<wchar_t> init, constructors and change_size (unit v5645;
// libtpw_unit_std_basic_string_init.dylib, tests/test_std_basic_string_init.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are fakes that record what the real ones read; texts are logged as an FNV-1a hash of their bytes,
// memory addresses never (they differ between the two sides). Blocks come with padding on both sides, because a huge length wraps
// the 32-bit address to a little before the block.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
char16_t gMslWcharNulA;
char16_t gMslWcharNulB;
}  // namespace IME

namespace {

int64_t Fnv(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

void* Block(uint32_t size) {
    uint32_t n = size < 256 ? 256 : (size > 1024 ? 1024 : size);
    return static_cast<char*>(tpw::ArenaAllocZeroed(n + 32)) + 16;
}

int64_t Id(const void* p) {
    if (p == IME::gMslLengthErrorVtbl) return 1;
    if (p == IME::gMslExcVtblB) return 2;
    if (p == IME::gMslExcVtblC) return 3;
    return 0;
}

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {Fnv(s), static_cast<int64_t>(n)});
    return n;
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {Fnv(s)});
    std::strcpy(d, s);
    return d;
}

void* MslNew(uint32_t size) {
    ev("new", {size});
    return Block(size);
}

void* MslNewRep() {
    ev("new", {0x10});
    return Block(0x10);
}

void* MslNewArray(uint32_t size) {
    ev("newarr", {size});
    return Block(size);
}

void MslDelete(void* p) {
    const char16_t* c = static_cast<const char16_t*>(p);
    ev("delete", {p ? c[0] : 0, p ? c[1] : 0});
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* ex = static_cast<MslExceptionObject*>(object);
    int64_t count = ex->mRc.mCount ? *ex->mRc.mCount : 0;
    ev("throw", {Fnv(typeName), Id(ex->mVtbl), Fnv(ex->mRc.mPtr), count, dtorTV == gMslLengthErrorDtorTV ? 1 : 0});
}

char16_t* MslWmemcpy(char16_t* d, const char16_t* s, uint32_t n) {
    int64_t c[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < 4 && i < n; ++i) c[i] = s[i];
    ev("wmemcpy", {n, c[0], c[1], c[2], c[3]});
    if (n <= 1024) std::memcpy(d, s, n * 2);
    return d;
}

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {p != nullptr ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}

}  // namespace IME

extern "C" {

uint64_t tpw_sbsi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"nulInit", &IME::gMslWcharNulInit},
        {"nulResize", &IME::gMslWcharNulResize},
        {"nulEmpty", &IME::gMslWcharNulEmpty},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sbsi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sbsi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mLength));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mCapacity));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    return n;
}

void tpw_sbsi_init(uint64_t self, uint64_t first, uint64_t last) {
    IME::MslWideString_init(reinterpret_cast<IME::MslWideString*>(self), reinterpret_cast<const char16_t*>(first),
                            reinterpret_cast<const char16_t*>(last));
}

uint64_t tpw_sbsi_ctor(uint64_t self, uint64_t text, uint32_t n) {
    return reinterpret_cast<uintptr_t>(IME::MslWideString_ctor(reinterpret_cast<IME::MslWideString*>(self),
                                                               reinterpret_cast<const char16_t*>(text), n));
}

uint64_t tpw_sbsi_ctorc(uint64_t self, uint64_t text) {
    return reinterpret_cast<uintptr_t>(
        IME::MslWideString_ctorCStr(reinterpret_cast<IME::MslWideString*>(self), reinterpret_cast<const char16_t*>(text)));
}

uint64_t tpw_sbsi_ctore(uint64_t self) {
    return reinterpret_cast<uintptr_t>(IME::MslWideString_ctorEmpty(reinterpret_cast<IME::MslWideString*>(self)));
}

void tpw_sbsi_change(uint64_t self, uint32_t n, uint32_t keep) {
    IME::MslWideString_changeSize(reinterpret_cast<IME::MslWideString*>(self), n, (keep & 0xff) != 0);
}

}  // extern "C"
