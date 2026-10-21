// Test library for std::basic_string<char> replace and init (unit v5941; libtpw_unit_std_basic_string_replace_2.dylib,
// tests/test_std_basic_string_replace_2.py), main app:
//   src/game/MslSupport.cpp   replace 0x10002d20, init 0x10072e6c; the three terminator globals.
//
// The callees outside the unit (strlen, strcpy, new, new[], delete, memcpy, memset, __throw) are recording fakes with the same
// events as the PowerPC stubs; memmove is the real one on both sides. Globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

int64_t Hash(const char* s) {
    uint32_t acc = 0;
    for (; *s != 0; ++s) acc = acc * 131u + static_cast<uint8_t>(*s);
    return acc;
}

int64_t VtblId(const void* p) {
    if (p == IME::gMslExcVtblA) return 1;
    if (p == IME::gMslExcVtblB) return 2;
    if (p == IME::gMslExcVtblC) return 3;
    if (p == IME::gMslLengthErrorVtbl) return 4;
    return 0;
}

int64_t DtorId(const void* p) {
    if (p == IME::gMslOutOfRangeDtorTV) return 1;
    if (p == IME::gMslLengthErrorDtorTV) return 2;
    return 0;
}

}  // namespace

namespace IME {
size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {static_cast<int64_t>(n)});
    return n;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {Hash(s)});
    return std::strcpy(d, s);
}
void* MslNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size == 0 ? 4 : size);
}
void* MslNewArray(uint32_t size) {
    ev("newa", {size});
    return tpw::ArenaAllocZeroed(size == 0 ? 4 : size);
}
void* MslNewRep() {
    ev("new", {0x10});
    return tpw::ArenaAllocZeroed(sizeof(MslWideStringRep) + 16);
}
void MslDelete(void* p) { ev("delete", {lab(p)}); }
void MslDeleteArray(void* p) { ev("deletea", {lab(p)}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {Hash(typeName), VtblId(o->mVtbl), DtorId(dtorTV), Hash(o->mRc.mPtr), static_cast<int64_t>(*o->mRc.mCount)});
}
}  // namespace IME

namespace msl_string_callee {
void* Memcpy(void* to, const void* from, uint32_t n) {
    ev("memcpy", {n});
    return std::memcpy(to, from, n);
}
void* Memset(void* to, int32_t c, uint32_t n) {
    ev("memset", {c, n});
    if (n > 0x1000000u) return to;   // the fake skips a huge fill on both sides
    return std::memset(to, c, n);
}
}  // namespace msl_string_callee

extern "C" {

uint64_t tpw_bsr2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblA", IME::gMslExcVtblA},
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
        {"vtblLE", IME::gMslLengthErrorVtbl},
        {"dtorOOR", IME::gMslOutOfRangeDtorTV},
        {"dtorLE", IME::gMslLengthErrorDtorTV},
        {"nulCopy", &gMslReplaceNulCopy},
        {"nulPlace", &gMslReplaceNulPlace},
        {"nulInit", &gMslInitFillNul},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bsr2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bsr2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, len));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, cap));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, refs));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, data));
    out[n++] = static_cast<int64_t>(sizeof(MslCharString));
    return n;
}

void tpw_bsr2_replace(uint64_t self, uint32_t pos, uint32_t n1, uint32_t n2, int32_t c) {
    msl_char_string::replace(reinterpret_cast<MslCharString*>(self), pos, n1, n2, static_cast<char>(c));
}

void tpw_bsr2_init(uint64_t self, uint32_t n, int32_t c) {
    msl_char_string::init(reinterpret_cast<MslCharString*>(self), n, static_cast<char>(c), nullptr);
}

}  // extern "C"
