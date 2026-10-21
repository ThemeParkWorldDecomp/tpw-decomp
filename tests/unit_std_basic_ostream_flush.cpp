// Test library for std::basic_ostream<char>::flush (unit v5663; libtpw_unit_std_basic_ostream_flush.dylib), main app:
//   src/game/MslSupport.cpp   OstreamFlush
//
// The callees outside the unit (ios_base::good, basic_ios::tie, ios_base::setstate, the stream buffer virtual, __throw and the
// allocator/string imports) are fakes recording what the real ones read; answers come from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_istream.h"
#include "ui_ime.h"

using namespace calllog;

char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];

namespace {
uint64_t gTieStream = 0;

int64_t Sum(const char* text) {
    int64_t sum = 0;
    for (; *text != 0; text++) sum = sum * 31 + static_cast<uint8_t>(*text);
    return sum & 0x7fffffff;
}
}  // namespace

bool MslIosGood(MacIosBase* ios) {
    ev("good", {lab(ios)});
    return ret("good") != 0;
}

void* MslIosTie(MacIosBase* ios) {
    ev("tie", {lab(ios)});
    return ret("tie") != 0 ? reinterpret_cast<void*>(static_cast<uintptr_t>(gTieStream)) : nullptr;
}

void MslIosSetstate(MacIosBase* ios, int32_t bits) { ev("setstate", {lab(ios), bits}); }

int32_t MslStreamBufVirtual(MacStreamBuf* sb, uint32_t entry) {
    ev("virt", {lab(sb), static_cast<int64_t>(entry)});
    return static_cast<int32_t>(ret("glue"));
}

void MslThrow(const char* type, MslThrowObject* object, const void* destructors) {
    ev("throw", {Sum(type), object->mVtbl == gIosFailureVtbl ? 1 : 0, static_cast<int64_t>(std::strlen(object->mMessage)),
                 Sum(object->mMessage), static_cast<int64_t>(*object->mCount), destructors == gIosFailureDtorVec ? 1 : 0});
}

namespace IME {
size_t MslStrlen(const char* s) { return std::strlen(s); }
char* MslStrcpy(char* d, const char* s) { return std::strcpy(d, s); }
void* MslNew(uint32_t size) { return std::calloc(size < 4 ? 4 : size, 1); }
void* MslNewArray(uint32_t size) { return std::calloc(size < 4 ? 4 : size, 1); }
}  // namespace IME

extern "C" {

uint64_t tpw_flush_addr(const char* name) {
    if (std::strcmp(name, "tie") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gTieStream));
    return 0;
}

uint64_t tpw_flush_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_flush_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_flush_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mState));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(MacSentryIos, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(BasicIstreamChar, mIos));
    return n;
}

uint64_t tpw_flush_call(uint64_t stream) {
    BasicIstreamChar* self = reinterpret_cast<BasicIstreamChar*>(static_cast<uintptr_t>(stream));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(OstreamFlush(self)));
}

}  // extern "C"
