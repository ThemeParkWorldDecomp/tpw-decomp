// Test library for the ride animation time statistics (unit v5576; libtpw_unit_GetAnimTimeStats.dylib, tests/test_GetAnimTimeStats.py), main app:
//   src/game/rides/RideAnim.cpp   GetAnimTimeStats, RestoreAnimTimeStats, GetNumActiveChannels
//
// No callees and no globals of other files.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"

extern "C" {

uint64_t tpw_gat_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gat_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeControls));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mAnimatedCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF10));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF14));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF20));
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControl));
    out[n++] = static_cast<int64_t>(offsetof(tag_sAnimHeader, mF04));
    out[n++] = static_cast<int64_t>(offsetof(tag_sAnimHeader, mF08));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    return n;
}

void tpw_gat_get(uint64_t control, uint64_t time) {
    GetAnimTimeStats(reinterpret_cast<sAnimTimeControl*>(control), reinterpret_cast<uint32_t*>(time));
}
void tpw_gat_restore(uint64_t control, uint64_t anim) {
    RestoreAnimTimeStats(reinterpret_cast<sAnimTimeControl*>(control), reinterpret_cast<tag_sAnimHeader*>(anim));
}
void tpw_gat_chan(uint64_t set) { GetNumActiveChannels(reinterpret_cast<tag_sRideAnimSet*>(set)); }

}  // extern "C"
