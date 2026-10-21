// Test library for std::basic_string<char> (unit v5253; libtpw_unit_std_basic_string_basic_string.dylib), main app:
//   src/game/MslSupport.cpp   constructors, destructor, init, assign, resize, append
//
// The callees outside the unit are fakes that record what the real ones read; the allocator fakes hand out zeroed blocks of at least 80
// bytes with 16 guard bytes before (the original wraps an address at 32 bits when n = 0xffffffff).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

int g_strlenCalls = 0;

uint32_t Sum(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += static_cast<uint32_t>(p[i]) * static_cast<uint32_t>(i + 1);
    return sum;
}

void* Blk(size_t size) {
    size_t n = size < 80 ? 80 : size;
    return static_cast<uint8_t*>(tpw::ArenaAllocZeroed(n + 32)) + 16;
}

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    if (g_strlenCalls++ == 0) {
        int64_t v = ret("strlen0");
        if (v) n = static_cast<uint32_t>(v);
    }
    ev("strlen", {static_cast<int64_t>(n)});
    return n;
}

char* MslStrcpy(char* d, const char* s) {
    size_t n = std::strlen(s);
    ev("strcpy", {static_cast<int64_t>(n), static_cast<int64_t>(Sum(s, n))});
    std::memcpy(d, s, n + 1);
    return d;
}

void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return Blk(size);
}

void* MslNewRep() {
    ev("new", {16});
    return Blk(24);
}

void* MslNewArray(uint32_t size) {
    ev("newarr", {static_cast<int64_t>(size)});
    return Blk(size);
}

void MslDelete(void* p) { ev("delete", {lab(p)}); }

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    MslExceptionObject* object = reinterpret_cast<MslExceptionObject*>(reinterpret_cast<char*>(self) - offsetof(MslExceptionObject, mRc));
    ev("refptr", {p != nullptr ? 1 : 0, object->mVtbl == gMslExcVtblB ? 1 : 0});
    self->mPtr = p;
    self->mCount = static_cast<int32_t*>(Blk(4));
    *self->mCount = 1;
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* e = static_cast<MslExceptionObject*>(object);
    size_t nameLen = std::strlen(typeName);
    size_t msgLen = std::strlen(e->mRc.mPtr);
    ev("throw", {static_cast<int64_t>(nameLen), static_cast<int64_t>(Sum(typeName, nameLen)), e->mVtbl == gMslLengthErrorVtbl ? 1 : 0,
                 static_cast<int64_t>(msgLen), static_cast<int64_t>(Sum(e->mRc.mPtr, msgLen)), dtorTV == gMslLengthErrorDtorTV ? 1 : 0});
}

}  // namespace IME

namespace msl_string_callee {

void* Memcpy(void* to, const void* from, uint32_t n) {
    ev("memcpy", {static_cast<int64_t>(n), n <= 4096 ? static_cast<int64_t>(Sum(from, n)) : 0});
    if (n <= 4096) std::memcpy(to, from, n);
    return to;
}

void* Memset(void* to, int32_t c, uint32_t n) {
    ev("memset", {c & 0xff, static_cast<int64_t>(n)});
    if (n <= 4096) std::memset(to, c, n);
    return to;
}

}  // namespace msl_string_callee

namespace msl_char_string {

void change_size(MslCharString* self, uint32_t n, bool keep) {
    ev("change_size", {lab(self), static_cast<int64_t>(n), keep ? 1 : 0});
    MslCharRep* old = self->mRep;
    MslCharRep* rep = static_cast<MslCharRep*>(Blk(24));
    char* data = static_cast<char*>(Blk(static_cast<size_t>(n) + 1));
    if (keep) {
        uint32_t m = old->len < n ? old->len : n;
        if (m) std::memcpy(data, old->data, m);
    }
    rep->len = n;
    rep->cap = n;
    rep->refs = 1;
    rep->data = data;
    self->mRep = rep;
}

}  // namespace msl_char_string

extern "C" {

void tpw_ssb_reset() { g_strlenCalls = 0; }

uint64_t tpw_ssb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"initnul", &gMslInitNul},
        {"ctornul", &gMslCtorNul},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ssb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ssb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslCharRep));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, data));
    out[n++] = static_cast<int64_t>(sizeof(MslCharString));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslExceptionObject, mRc));
    return n;
}

uint64_t tpw_ssb_ctor_c(uint64_t self, uint64_t text) {
    return reinterpret_cast<uintptr_t>(
        msl_char_string::constructFromCStr(reinterpret_cast<MslCharString*>(self), reinterpret_cast<const char*>(text), nullptr));
}
uint64_t tpw_ssb_ctor0(uint64_t self) { return reinterpret_cast<uintptr_t>(msl_char_string::constructEmpty(reinterpret_cast<MslCharString*>(self))); }
uint64_t tpw_ssb_dtor(uint64_t self, int16_t flag) {
    return reinterpret_cast<uintptr_t>(msl_char_string::destroy(reinterpret_cast<MslCharString*>(self), flag));
}
void tpw_ssb_init(uint64_t self, uint64_t first, uint64_t last) {
    msl_char_string::init(reinterpret_cast<MslCharString*>(self), reinterpret_cast<const char*>(first), reinterpret_cast<const char*>(last));
}
uint64_t tpw_ssb_fill(uint64_t self, uint32_t n, uint8_t c) {
    return reinterpret_cast<uintptr_t>(msl_char_string::assignFill(reinterpret_cast<MslCharString*>(self), n, static_cast<char>(c)));
}
void tpw_ssb_resize(uint64_t self, uint32_t n, uint8_t c) {
    msl_char_string::resize(reinterpret_cast<MslCharString*>(self), n, static_cast<char>(c));
}
uint64_t tpw_ssb_chars(uint64_t self, uint64_t text, uint32_t n) {
    return reinterpret_cast<uintptr_t>(
        msl_char_string::assignChars(reinterpret_cast<MslCharString*>(self), reinterpret_cast<const char*>(text), n));
}
uint64_t tpw_ssb_append(uint64_t self, uint64_t text, uint32_t n) {
    return reinterpret_cast<uintptr_t>(
        msl_char_string::appendChars(reinterpret_cast<MslCharString*>(self), reinterpret_cast<const char*>(text), n));
}

}  // extern "C"
