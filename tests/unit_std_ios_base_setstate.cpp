// Test library for std::ios_base::setstate (unit v5523; libtpw_unit_std_ios_base_setstate.dylib, tests/test_std_ios_base_setstate.py):
//   src/std_ios_base_setstate.cpp   MslIosSetstate
//
// __throw is a fake recording what the thrown object holds; the failure vtable globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_istream.h"

using namespace calllog;

char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];

// The message bytes as big-endian words, zero padded: the PowerPC stub builds the same ints.
void MslThrow(const char* type, MslThrowObject* object, const void* destructors) {
    int64_t w[7] = {0, 0, 0, 0, 0, 0, 0};
    size_t n = std::strlen(object->mMessage) + 1;
    for (size_t i = 0; i < n && i < 28; i++) w[i / 4] |= static_cast<int64_t>(static_cast<uint8_t>(object->mMessage[i])) << (24 - 8 * (i % 4));
    bool typeOk = std::strcmp(type, "!std::exception!!std::ios_base::failure!!") == 0;
    ev("__throw", {typeOk ? 1 : 0, object->mVtbl == gIosFailureVtbl ? 1 : 0, destructors == gIosFailureDtorVec ? 1 : 0,
                   static_cast<int64_t>(*object->mCount), w[0], w[1], w[2], w[3], w[4], w[5], w[6]});
}

extern "C" {

uint64_t tpw_setstate_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_setstate_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mState));
    out[n++] = static_cast<int64_t>(offsetof(MacIosBase, mExceptions));
    return n;
}

void tpw_setstate_call(uint64_t ios, int32_t bits) { MslIosSetstate(reinterpret_cast<MacIosBase*>(static_cast<uintptr_t>(ios)), bits); }

}  // extern "C"
