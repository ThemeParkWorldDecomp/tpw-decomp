// Test library for std::basic_ostream<wchar_t>::flush and constructor (unit v5898; libtpw_unit_std_basic_ostream_flush_2.dylib,
// tests/test_std_basic_ostream_flush_2.py), main app:
//   src/game/MslSupport.cpp   WostreamFlush, WostreamConstruct, gWostreamVtbl
//
// The callees are recording fakes; globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"

using namespace calllog;

char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];
char gIosBaseVt[16];
uint32_t gIfsWVtblE[4];

namespace {
void* gTieTarget = nullptr;

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return h;
}
}  // namespace

namespace wostream_u {
bool Good(void* ios) {
    ev("Good", {lab(ios)});
    return ret("Good") != 0;
}
void* Tie(void* ios) {
    ev("Tie", {lab(ios)});
    return ret("Tie") != 0 ? gTieTarget : nullptr;
}
void Setstate(void* ios, int32_t bits) { ev("Setstate", {lab(ios), bits}); }
int32_t Virt(MacStreamBuf* sb, uint32_t entry) {
    ev("Virt", {lab(sb), static_cast<int64_t>(entry)});
    return static_cast<int32_t>(ret("Virt"));
}
void Throw(const char* type, MslThrowObject* object, const void* destructors) {
    int64_t count = object->mCount != nullptr ? static_cast<int64_t>(static_cast<uint32_t>(*object->mCount)) : -1;
    ev("Throw", {Hash(type), object->mVtbl == gIosFailureVtbl ? 1 : 0, object->mMessage ? Hash(object->mMessage) : -2, count,
                 destructors == gIosFailureDtorVec ? 1 : 0});
}
char* NewArray(uint32_t size) {
    ev("NewArray", {static_cast<int64_t>(size)});
    return static_cast<char*>(tpw::ArenaAllocZeroed(size));
}
int32_t* NewInt() {
    ev("New", {4});
    return static_cast<int32_t*>(tpw::ArenaAllocZeroed(4));
}
void IosInit(void* ios, uint32_t streambuf) { ev("Init", {lab(ios), static_cast<int64_t>(streambuf)}); }
char16_t Widen(void* ios, char c) {
    ev("Widen", {lab(ios), static_cast<uint8_t>(c)});
    return static_cast<char16_t>(ret("Widen") & 0xffff);
}
}  // namespace wostream_u

extern "C" {

uint64_t tpw_wf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gIosFailureVtbl", gIosFailureVtbl}, {"gIosFailureDtorVec", gIosFailureDtorVec}, {"gIosBaseVt", gIosBaseVt},
        {"gIfsWVtblE", gIfsWVtblE},           {"gWostreamVtbl", gWostreamVtbl},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_wf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_wf_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }
void tpw_wf_settie(uint64_t p) { gTieTarget = reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }

int32_t tpw_wf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mState));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mExceptions));
    out[n++] = static_cast<int64_t>(sizeof(WostreamW));
    return n;
}

uint64_t tpw_wf_flush(uint64_t self) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(WostreamFlush(reinterpret_cast<WostreamW*>(static_cast<uintptr_t>(self)))));
}
uint64_t tpw_wf_ctor(uint64_t self, int32_t flag, uint32_t streambuf) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        WostreamConstruct(reinterpret_cast<WostreamW*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag), streambuf)));
}

}  // extern "C"
