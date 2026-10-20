// Test library for the ostream sentry destructor (unit v5251; libtpw_unit_std_basic_ostream_sentry_Dtor_sentry.dylib), main app:
//   src/std_basic_ostream_sentry_Dtor_sentry.cpp   OstreamSentryDtor
//
// The callees outside the unit (flush, operator delete) are fakes recording their pointer as an offset from the case's block.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_istream.h"

using namespace calllog;

namespace {
uintptr_t gBase = 0;
}

void MslOstreamFlush(void* stream) { ev("flush", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(stream) - gBase)}); }
void MslOperatorDelete(void* p) { ev("delete", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - gBase)}); }

extern "C" {

uint64_t tpw_ostsent_alloc(uint32_t size) {
    void* p = tpw::ArenaAllocZeroed(size);
    gBase = reinterpret_cast<uintptr_t>(p);
    return static_cast<uint64_t>(gBase);
}

// The Ptr32 field value for an arena address.
uint32_t tpw_ostsent_off(uint64_t addr) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(addr))); }

int32_t tpw_ostsent_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacSentryObject, mDone));
    out[n++] = static_cast<int64_t>(offsetof(MacSentryObject, mStream));
    out[n++] = static_cast<int64_t>(offsetof(MacSentryIos, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(MacSentryIos, mState));
    return n;
}

// Answers 1 when the function returned its argument.
uint32_t tpw_ostsent_dtor(uint64_t self, int32_t flag) {
    MacSentryObject* s = reinterpret_cast<MacSentryObject*>(static_cast<uintptr_t>(self));
    return OstreamSentryDtor(s, static_cast<int16_t>(flag)) == s ? 1 : 0;
}

}  // extern "C"
