// Test library for SetRideMobile (unit v5306; libtpw_unit_SetRideMobile.dylib, tests/test_SetRideMobile.py), main app:
//   src/TPEngine_SetRideMobile.cpp   SetRideMobile
//
// No callees. gRideAnimSets (defined in src/RideTypes.cpp, not compiled here) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "MainWindowCallback.h"

using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[16];

extern "C" {

uint64_t tpw_sr_addr(const char* name) {
    if (std::strcmp(name, "sets") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gRideAnimSets[0]));
    return 0;
}

uint64_t tpw_sr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word for a host address.
uint32_t tpw_sr_p32(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

// set mFlags, set mHeads, mesh mHeader, header mFlags, header mF78, pointer size
int32_t tpw_sr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_sr_set_ride_mobile(uint64_t ride) { SetRideMobile(*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ride))); }

}  // extern "C"
