// Test library for std::basic_string<char> assign(str, pos, n) and change_size (unit v5376; libtpw_unit_std_basic_string_assign.dylib,
// tests/test_std_basic_string_assign.py), main app:
//   src/game/MslSupport.cpp   msl_char_string::assign, msl_char_string::change_size
//
// The callees outside the unit are fakes that record what the real ones read (pointers of the world as labels, copied bytes and message
// texts as checksums); the fake __throw returns. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "ui_ime.h"

using namespace calllog;

char gMslChangeSizeNul;
namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

int64_t Sum(const char* bytes, size_t n) {
    uint32_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum += static_cast<uint32_t>(i + 1) * static_cast<uint8_t>(bytes[i]);
    return sum;
}

int64_t VtblId(const void* v) {
    if (v == IME::gMslExcVtblA) return 1;
    if (v == IME::gMslExcVtblB) return 2;
    if (v == IME::gMslExcVtblC) return 3;
    if (v == IME::gMslLengthErrorVtbl) return 4;
    return 0;
}

void* Alloc(uint32_t size) { return tpw::ArenaAllocZeroed(size < 4 ? 4 : size); }

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {static_cast<int64_t>(n)});
    return n;
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {static_cast<int64_t>(std::strlen(s))});
    std::strcpy(d, s);
    return d;
}

void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return Alloc(size);
}

void* MslNewArray(uint32_t size) {
    ev("newa", {static_cast<int64_t>(size)});
    return Alloc(size);
}

void* MslNewRep() {
    ev("new", {16});
    return Alloc(sizeof(MslCharRep));
}

void MslDelete(void* p) { ev("delete", {static_cast<int64_t>(lab(p))}); }

void MslDeleteArray(void* p) { ev("deletea", {static_cast<int64_t>(lab(p))}); }

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* o = static_cast<const MslExceptionObject*>(object);
    int64_t dtor = dtorTV == gMslOutOfRangeDtorTV ? 1 : dtorTV == gMslLengthErrorDtorTV ? 2 : 0;
    ev("throw", {Sum(typeName, std::strlen(typeName)), VtblId(o->mVtbl), Sum(o->mRc.mPtr, std::strlen(o->mRc.mPtr)), dtor});
}

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {p != nullptr ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}

}  // namespace IME

namespace msl_string_callee {
void* Memcpy(void* to, const void* from, uint32_t n) {
    ev("memcpy", {static_cast<int64_t>(lab(to)), static_cast<int64_t>(n), Sum(static_cast<const char*>(from), n < 4096 ? n : 4096)});
    if (n) std::memmove(to, from, n);
    return to;
}
}  // namespace msl_string_callee

namespace msl_char_string {
void replace(MslCharString* self, uint32_t pos, uint32_t n1, uint32_t n2, char c) {
    ev("replace", {static_cast<int64_t>(lab(self)), static_cast<int64_t>(pos), static_cast<int64_t>(n1), static_cast<int64_t>(n2),
                   static_cast<int64_t>(static_cast<uint8_t>(c))});
}
}  // namespace msl_char_string

extern "C" {

uint64_t tpw_sba_addr(const char* name) {
    if (std::strcmp(name, "nul") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gMslChangeSizeNul));
    return 0;
}

uint64_t tpw_sba_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sba_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, len));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, cap));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, refs));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, data));
    out[n++] = static_cast<int64_t>(sizeof(MslCharRep));
    return n;
}

uint64_t tpw_sba_assign(uint64_t self, uint64_t src, uint32_t pos, uint32_t n) {
    MslCharString* r = msl_char_string::assign(reinterpret_cast<MslCharString*>(static_cast<uintptr_t>(self)),
                                               reinterpret_cast<const MslCharString*>(static_cast<uintptr_t>(src)), pos, n);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

void tpw_sba_change_size(uint64_t self, uint32_t n, uint32_t keep) {
    msl_char_string::change_size(reinterpret_cast<MslCharString*>(static_cast<uintptr_t>(self)), n, (keep & 0xFF) != 0);
}

}  // extern "C"
