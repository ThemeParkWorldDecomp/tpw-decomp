// Test library for std::basic_istream<wchar_t> ~basic_istream, getline and the constructor (unit v5708;
// libtpw_unit_std_basic_istream_Dtor_basic_istream_2.dylib, tests/test_std_basic_istream_Dtor_basic_istream_2.py), main app:
//   src/COnlineControl_..._SaveIgnore_std_basic_istream_Dtor_basic_istream_2.cpp
//
// The callees outside the unit are recording fakes that record what the PowerPC stubs record. The globals of other files
// the unit reads (the vtables and the failure objects' addresses) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "online.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"

using namespace calllog;

// ---- globals of other files, defined here
uint32_t gIfsWVtblA[4];
uint32_t gIfsWVtblB[4];
uint32_t gIfsWVtblC[4];
uint32_t gIfsWVtblD[4];
uint32_t gIfsWVtblE[4];
uint8_t gIfsWFiles[0xfc];
char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];

namespace {

// A text as one number: the same fold on both sides.
int64_t TextCode(const char* text) {
    uint32_t h = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; p++) h = h * 31 + *p;
    return h;
}

constexpr uint32_t kPeekEntry = 0xA028;

}  // namespace

// ---- the fakes
namespace wostream_callee {
bool Good(void* ios) {
    ev("Good", {lab(ios)});
    return ret("Good") != 0;
}
void* Tie(void* ios) {
    ev("Tie", {lab(ios)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("Tie"))));
}
void Flush(void* stream) { ev("Flush", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(stream) & 0xffffffffu)}); }
void Setstate(void* ios, int32_t bits) { ev("Setstate", {lab(ios), static_cast<uint32_t>(bits)}); }
uint32_t Strlen(const char* text) {
    ev("Strlen", {TextCode(text)});
    return static_cast<uint32_t>(std::strlen(text));
}
char* NewBytes(uint32_t size) {
    ev("NewArray", {size});
    return static_cast<char*>(tpw::ArenaAllocZeroed(size < 4 ? 4 : size));
}
int32_t* NewInt() {
    ev("New", {4});
    return static_cast<int32_t*>(tpw::ArenaAllocZeroed(4));
}
void CopyString(char* to, const char* from) {
    ev("Strcpy", {TextCode(from), to != nullptr});
    if (to != nullptr) std::strcpy(to, from);
}
void Throw(const char* type, void* object, const void* destructors) {
    MslThrowObject* o = static_cast<MslThrowObject*>(object);
    ev("Throw", {TextCode(type), o->mVtbl == static_cast<const void*>(gIosFailureVtbl), TextCode(o->mMessage),
                 *o->mCount, destructors == static_cast<const void*>(gIosFailureDtorVec)});
}
}  // namespace wostream_callee

namespace wistream_callee {
void IosInit(void* ios, uint32_t streambuf) { ev("IosInit", {lab(ios), streambuf}); }
char16_t Widen(void* ios, char c) {
    ev("Widen", {lab(ios), static_cast<uint8_t>(c)});
    return static_cast<char16_t>(ret("Widen"));
}
}  // namespace wistream_callee

namespace ifstream_callee {
void IosBaseDtor(void* ios, int32_t flag) { ev("IosDtor", {lab(ios), static_cast<uint32_t>(flag)}); }
}  // namespace ifstream_callee

void MslOperatorDelete(void* p) { ev("Delete", {lab(p)}); }

int32_t MslStreamBufVirtual(MacStreamBuf* sb, uint32_t entry) {
    ev("PtrGlue", {lab(sb), entry});
    return static_cast<int32_t>(ret(entry == kPeekEntry ? "Peek" : "Bump"));
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_wi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"A", gIfsWVtblA},
        {"E", gIfsWVtblE},
        {"FV", gIosFailureVtbl},
        {"FD", gIosFailureDtorVec},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_wi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The 32-bit arena offset of a block (what a Ptr32 holds).
uint32_t tpw_wi_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_wi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(WideIstream, mGcount));
    out[n++] = static_cast<int64_t>(offsetof(WideIstream, mVtable));
    out[n++] = static_cast<int64_t>(kWideIstreamIosPart);
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mPad3c));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mBaseOffset));
    return n;
}

uint64_t tpw_wi_dtor(uint64_t stream, int16_t flag) {
    ignore_callee::IstreamDtor(reinterpret_cast<ignore_callee::IgnoreStream*>(static_cast<uintptr_t>(stream)), flag);
    return stream;   // the original answers this; the declaration is void
}

uint64_t tpw_wi_ctor(uint64_t stream, int16_t flag, uint32_t streambuf) {
    return reinterpret_cast<uintptr_t>(ignore_callee::IstreamCtor(
        reinterpret_cast<ignore_callee::IgnoreStream*>(static_cast<uintptr_t>(stream)), flag, streambuf));
}

uint64_t tpw_wi_getline(uint64_t stream, uint64_t buffer, int32_t count, uint16_t delimiter) {
    return reinterpret_cast<uintptr_t>(ignore_callee::IstreamGetline(
        reinterpret_cast<ignore_callee::IgnoreStream*>(static_cast<uintptr_t>(stream)),
        reinterpret_cast<char16_t*>(static_cast<uintptr_t>(buffer)), count, delimiter));
}

}  // extern "C"
