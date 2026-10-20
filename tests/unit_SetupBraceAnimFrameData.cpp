// Test library for SetupBraceAnimFrameData (unit v5214; libtpw_unit_SetupBraceAnimFrameData.dylib, tests/test_SetupBraceAnimFrameData.py), main app:
//   src/GetTangentsFromZVector_SetupBraceAnimFrameData.cpp   SetupBraceAnimFrameData
//
// The only callee outside the unit is modf (the C library, run natively here and by the test's Python on the PowerPC side).
// gRideAnimSets (src/RideTypes.cpp) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "TPCoaster.h"

using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[64];

extern "C" {

uint64_t tpw_sb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFE8w));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFEC));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFF0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFF4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFF8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF18Table));
    out[n++] = static_cast<int64_t>(sizeof(sTrackBraceData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeControls));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    return n;
}

void tpw_sb_setup(uint64_t brace) {
    SetupBraceAnimFrameData(nullptr, reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace)));
}

}  // extern "C"
