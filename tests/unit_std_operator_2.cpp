// Test library for std::operator<<(basic_ostream<char>&, const char*) (unit v5700; libtpw_unit_std_operator_2.dylib,
// tests/test_std_operator_2.py), main app:
//   src/email_load_std_operator_2.cpp   OstreamWriteText
//
// The callees outside the unit (good, tie, flush, setstate, __putnumber, __throw, strlen, strcpy, operator new / new[]) are recording
// fakes with the same event names and arguments as the module's PowerPC stubs. Pointers are logged as codes (0 NULL, 1 ios, 2 stream,
// 3 stream buffer, -1 other); texts as an FNV-1a hash and their length. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "ui_ime.h"

using namespace calllog;

char gIosFailureVtbl[16];       // 0x1029d084
char gIosFailureDtorVec[16];    // 0x102661d0

namespace {

uint64_t gIos = 0, gOs = 0, gSb = 0;

int64_t Code(const void* p) {
    uint64_t v = reinterpret_cast<uint64_t>(p);
    if (v == 0) return 0;
    if (v == gIos) return 1;
    if (v == gOs) return 2;
    if (v == gSb) return 3;
    return -1;
}

int64_t Hash(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

}  // namespace

// ---- the fakes
bool MslIosGood(MacIosBase* ios) {
    ev("good", {Code(ios)});
    return ret("good") != 0;
}
void* MslIosTie(MacIosBase* ios) {
    ev("tie", {Code(ios)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("tie")));
}
void MslOstreamFlush(void* stream) { ev("flush", {Code(stream), static_cast<int64_t>(reinterpret_cast<uintptr_t>(stream) < 0x10000 ? reinterpret_cast<uintptr_t>(stream) : 0)}); }
void MslIosSetstate(MacIosBase* ios, int32_t bits) { ev("setstate", {Code(ios), bits}); }

MacPutStreamBuf* PutNumberChar(MacPutStreamBuf* iter, MacPutIos* ios, char fill, const char* prefix, int32_t prefixLen,
                               const char* suffix, int32_t suffixLen) {
    ev("putnumber", {Code(iter), Code(ios), static_cast<uint8_t>(fill), Code(prefix), prefixLen, Hash(suffix), suffixLen,
                     static_cast<int64_t>(ios->mWidth)});
    return ret("put") != 0 ? iter : nullptr;
}

void MslThrow(const char* type, MslThrowObject* object, const void* destructors) {
    int64_t count = object->mCount ? *object->mCount : -7;
    ev("throw", {Hash(type), object->mVtbl == gIosFailureVtbl ? 1 : 0, object->mMessage ? Hash(object->mMessage) : -1,
                 object->mCount ? 1 : 0, count, destructors == gIosFailureDtorVec ? 1 : 0});
}

namespace IME {
size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {Hash(s), static_cast<int64_t>(n)});
    return n;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {Hash(s)});
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
}  // namespace IME

extern "C" {

uint64_t tpw_ow_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtbl", gIosFailureVtbl},
        {"vec", gIosFailureDtorVec},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ow_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ow_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_ow_expect(uint64_t ios, uint64_t os, uint64_t sb) {
    gIos = ios;
    gOs = os;
    gSb = sb;
}

int32_t tpw_ow_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacWriteIos, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(MacWriteIos, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(MacWriteIos, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(MacWriteIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(MacWriteIos, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(MacWriteIos, mFill));
    return n;
}

// Answers 1 when the stream comes back.
uint32_t tpw_ow_write(uint64_t os, uint64_t text) {
    MacWriteStream* s = reinterpret_cast<MacWriteStream*>(static_cast<uintptr_t>(os));
    return OstreamWriteText(s, reinterpret_cast<const char*>(static_cast<uintptr_t>(text))) == s ? 1 : 0;
}

}  // extern "C"
