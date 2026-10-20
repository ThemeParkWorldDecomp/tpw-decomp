// Test library for std::basic_istream<char>::read and ::getline (unit v5014; libtpw_unit_std_basic_istream_read.dylib,
// tests/test_std_basic_istream_read.py), main app: src/std_basic_istream_read.cpp.
//
// The callees outside the unit are recording fakes answering from the test's queues; the two globals of the exception object are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_istream.h"

using namespace calllog;

char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];

namespace {

int64_t Csum(const char* text) {
    int64_t sum = 0;
    for (int64_t i = 0; text[i] != 0; i++) sum += (i + 1) * static_cast<uint8_t>(text[i]);
    return sum & 0xFFFFFFFFll;
}

}  // namespace

bool MslIosGood(MacIosBase* ios) {
    ev("good", {static_cast<int64_t>(ios->mState)});
    return (ret("good") & 1) != 0;
}

void* MslIosTie(MacIosBase* ios) {
    ev("tie", {static_cast<int64_t>(ios->mState)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("tie") & 0xFFFFFFFFu));
}

void MslOstreamFlush(void* stream) { ev("flush", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(stream) & 0xFFFFFFFFu)}); }

void MslIosSetstate(MacIosBase* ios, int32_t bits) { ev("setstate", {static_cast<int64_t>(ios->mState), static_cast<int64_t>(static_cast<uint32_t>(bits))}); }

int32_t MslStreamBufVirtual(MacStreamBuf*, uint32_t entry) {
    ev("vcall", {static_cast<int64_t>(entry)});
    return static_cast<int32_t>(ret(entry == 0x0A000028u ? "v28" : "v2c"));
}

void MslThrow(const char* type, MslThrowObject* object, const void* destructors) {
    int64_t vt = 0, count = 0, msg = 0;
    if (object) {
        vt = object->mVtbl == gIosFailureVtbl ? 1 : 0;
        count = *object->mCount;
        msg = Csum(object->mMessage);
    }
    ev("throw", {Csum(type), vt, count, msg, destructors == gIosFailureDtorVec ? 1 : 0});
}

extern "C" {

uint64_t tpw_isb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_isb_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_isb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mState));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(BasicIstreamChar, mGcount));
    out[n++] = static_cast<int64_t>(offsetof(MacStreamBuf, mGptr));
    out[n++] = static_cast<int64_t>(offsetof(MacStreamBuf, mEgptr));
    out[n++] = static_cast<int64_t>(sizeof(BasicIstreamChar));
    return n;
}

uint64_t tpw_isb_read(uint64_t self, uint64_t buffer, int32_t count) {
    BasicIstreamChar* s = reinterpret_cast<BasicIstreamChar*>(static_cast<uintptr_t>(self));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(s->read(reinterpret_cast<char*>(static_cast<uintptr_t>(buffer)), count)));
}

uint64_t tpw_isb_getline(uint64_t self, uint64_t buffer, int32_t count, int32_t delimiter) {
    BasicIstreamChar* s = reinterpret_cast<BasicIstreamChar*>(static_cast<uintptr_t>(self));
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(s->getline(reinterpret_cast<char*>(static_cast<uintptr_t>(buffer)), count, static_cast<char>(delimiter))));
}

}  // extern "C"
