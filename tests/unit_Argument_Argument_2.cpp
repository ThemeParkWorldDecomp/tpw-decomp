// Test library for Argument::Argument(const Argument&) (unit v5242; libtpw_unit_Argument_Argument_2.dylib, tests/test_Argument_Argument_2.py), main app:
//   src/Argument_Argument_2.cpp   std_vector_callee::ArgumentCopy
//
// The callees outside the unit (strlen, wcslen, new[], strcpy, __throw, the ref-counted pointer constructor and the two string init
// functions) are fakes that record what the real ones read. strlen and wcslen answer a huge length for a marker string, so the throw
// path can be reached with small buffers. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

uintptr_t gDest = 0;
void* gLastNew = nullptr;

uint32_t Fnv(const void* p, int width, uint64_t limit) {
    uint32_t h = 2166136261u;
    for (uint64_t i = 0; i < limit && i < 4096; i++) {
        uint32_t c = width == 1 ? static_cast<const uint8_t*>(p)[i] : static_cast<const char16_t*>(p)[i];
        if (c == 0) break;
        h = (h ^ c) * 16777619u;
    }
    return h;
}

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    ev("strlen", {static_cast<int64_t>(Fnv(s, 1, 4096))});
    if (static_cast<uint8_t>(s[0]) == 1) {
        if (s[1] == 0x42) return 0xFFFFFFFFu;
        if (s[1] == 0x43) return 0xFFFFFFFEu;
    }
    return std::strlen(s);
}

void* MslNewArray(uint32_t size) {
    ev("newa", {static_cast<int64_t>(size)});
    gLastNew = tpw::ArenaAllocZeroed(size);
    return gLastNew;
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {d == gLastNew ? 1 : 0, static_cast<int64_t>(Fnv(s, 1, 4096))});
    std::strcpy(d, s);
    return d;
}

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {p == gLastNew ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {static_cast<int64_t>(Fnv(typeName, 1, 4096)), o->mVtbl == gMslLengthErrorVtbl ? 1 : 0,
                 static_cast<int64_t>(Fnv(o->mRc.mPtr, 1, 4096)), dtorTV == gMslLengthErrorDtorTV ? 1 : 0});
}

void MslWideString_init(MslWideString* self, const char16_t* first, const char16_t* last) {
    uint32_t diff = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last) - reinterpret_cast<uintptr_t>(first));
    ev("winit", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(self) - gDest), static_cast<int64_t>(diff),
                 static_cast<int64_t>(Fnv(first, 2, diff / 2))});
}

}  // namespace IME

namespace online_parser_callee {
uint32_t WcsLen(const char16_t* text) {
    uint32_t h = 2166136261u;
    uint32_t n = 0;
    while (n < 4096 && text[n]) h = (h ^ text[n]) * 16777619u, n++;
    ev("wcslen", {static_cast<int64_t>(h)});
    if (text[0] == 1) {
        if (text[1] == 0x4200) return 0x7FFFFFFFu;
        if (text[1] == 0x4300) return 0x7FFFFFFEu;
        if (text[1] == 0x4400) return 0xFFFFFFFFu;
    }
    uint32_t len = 0;
    while (text[len]) len++;
    return len;
}
}  // namespace online_parser_callee

namespace argument_callee {
void StringInit(void* self, const char* first, const char* last) {
    uint32_t diff = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last) - reinterpret_cast<uintptr_t>(first));
    ev("sinit", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(self) - gDest), static_cast<int64_t>(diff),
                 static_cast<int64_t>(Fnv(first, 1, diff))});
}
}  // namespace argument_callee

extern "C" {

uint64_t tpw_ac_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena base: the Argument's string fields hold offsets from it on the host.
uint64_t tpw_ac_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_ac_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Argument, mCommand));
    out[n++] = static_cast<int64_t>(sizeof(Argument));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    return n;
}

void tpw_ac_copy(uint64_t dest, uint64_t src) {
    gDest = static_cast<uintptr_t>(dest);
    gLastNew = nullptr;
    std_vector_callee::ArgumentCopy(reinterpret_cast<Argument*>(dest), reinterpret_cast<const Argument*>(src));
}

}  // extern "C"
