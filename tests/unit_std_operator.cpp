// Test library for operator<<(wostream&, const wchar_t*) (unit v5640; libtpw_unit_std_operator.dylib, tests/test_std_operator.py), main app:
//   src/game/MslSupport.cpp   ignore_callee::OstreamWrite
//
// The callees are fakes that record the arguments the real ones read and answer from the test's queues; the failure globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "std_basic_istream.h"

using namespace calllog;

char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];

namespace {
int64_t HashA(const char* s) {
    uint64_t h = 0;
    for (; *s; ++s) h = (h * 31 + static_cast<uint8_t>(*s)) & 0x7fffffffu;
    return static_cast<int64_t>(h);
}
int64_t HashW(const char16_t* s, int32_t n) {
    uint64_t h = 0;
    for (int32_t i = 0; i < n; ++i) h = (h * 31 + s[i]) & 0x7fffffffu;
    return static_cast<int64_t>(h);
}
}  // namespace

namespace wostream_callee {
bool Good(void* ios) {
    ev("Good", {lab(ios)});
    return ret("good") != 0;
}
void* Tie(void* ios) {
    ev("Tie", {lab(ios)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("tie")));
}
void Flush(void* stream) { ev("Flush", {lab(stream)}); }
void Setstate(void* ios, int32_t bits) { ev("Setstate", {lab(ios), bits}); }
void* PutNumber(void* iter, void* ios, char16_t fill, const char16_t* prefix, int32_t prefixLen, const char16_t* suffix, int32_t suffixLen) {
    ev("PutNumber", {lab(iter), lab(ios), fill, prefix ? 1 : 0, prefixLen, HashW(suffix, suffixLen), suffixLen});
    return ret("put") != 0 ? reinterpret_cast<void*>(static_cast<uintptr_t>(1)) : nullptr;
}
void Throw(const char* type, void* object, const void* destructors) {
    const auto* o = static_cast<const MslThrowObject*>(object);
    ev("Throw", {HashA(type), lab(o->mVtbl), HashA(o->mMessage), o->mCount ? *o->mCount : -1, lab(destructors)});
}
uint32_t Wcslen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n]) ++n;
    return n;
}
uint32_t Strlen(const char* text) { return static_cast<uint32_t>(std::strlen(text)); }
char* NewBytes(uint32_t size) { return static_cast<char*>(tpw::ArenaAllocZeroed(size)); }
int32_t* NewInt() { return static_cast<int32_t*>(tpw::ArenaAllocZeroed(4)); }
void CopyString(char* to, const char* from) { std::strcpy(to, from); }
}  // namespace wostream_callee

extern "C" {

uint64_t tpw_wo_addr(const char* name) {
    if (std::strcmp(name, "vtbl") == 0) return reinterpret_cast<uintptr_t>(gIosFailureVtbl);
    if (std::strcmp(name, "dtors") == 0) return reinterpret_cast<uintptr_t>(gIosFailureDtorVec);
    return 0;
}

uint64_t tpw_wo_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_wo_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mPad3c));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mPad28));
    return n;
}

uint64_t tpw_wo_write(uint64_t stream, uint64_t text) {
    ignore_callee::OstreamWrite(reinterpret_cast<ignore_callee::IgnoreStream*>(stream), reinterpret_cast<const char16_t*>(text));
    return stream;
}

}  // extern "C"
