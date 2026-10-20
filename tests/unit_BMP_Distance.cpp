// Test library for the bump sound and distance helpers (unit v5706; libtpw_unit_BMP_Distance.dylib, tests/test_BMP_Distance.py), main app:
//   src/BMP_InsideArea_PointOutAllInverse_BMP_Distance.cpp   BMP_Distance, PossitionSound1, SingleSoundAtBump, MineToSystemCords, NormaliseVector
//
// SOUND_MoveSoundEffect and SOUND_SendEvent are recording fakes; gSoundBanks (other file's global) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "Advisor.h"
#include "TPSound.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

int32_t gSoundBanks[8];

int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_MoveSoundEffect", {handle, a, b, c});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect"));
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("SOUND_SendEvent"));
}

extern "C" {

uint64_t tpw_bd_addr(const char* name) {
    if (std::strcmp(name, "banks") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSoundBanks[0]));
    return 0;
}

uint64_t tpw_bd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, x));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, y));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMP));
    out[n++] = static_cast<int64_t>(sizeof(s3DPoint));
    return n;
}

int32_t tpw_bd_dist(int32_t dx, int32_t dy) { return BMP_Distance(dx, dy); }
void tpw_bd_pos(uint64_t bump) { PossitionSound1(reinterpret_cast<tagBUMP*>(bump)); }
void tpw_bd_single(uint64_t bump, int32_t event) { SingleSoundAtBump(reinterpret_cast<tagBUMP*>(bump), event); }
void tpw_bd_mine(int32_t x, int32_t y, uint64_t out) { MineToSystemCords(x, y, reinterpret_cast<NS_MacDoze::tagPOINT*>(out)); }
void tpw_bd_norm(uint64_t v) { NormaliseVector(reinterpret_cast<s3DPoint*>(v)); }

}  // extern "C"
