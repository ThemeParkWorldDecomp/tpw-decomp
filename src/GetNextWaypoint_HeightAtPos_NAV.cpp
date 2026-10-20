// Unit v5216: HeightAtPos_NAV (0x10074230) and GetNextHoldingWaypoint (0x10074458).
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"

// 0x1029d2bc (initially 0x36b0, name invented): the lowest height a holding waypoint takes.
int32_t gFlyHoldMax = 0x36b0;

namespace {

int32_t FlyField(void* base, int32_t offset) {
    return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + offset);
}

}  // namespace

// The fly bases are native pointers here; the Mac's are 32-bit. Fields at +0x14 x, +0x18 height, +0x1c z.
int32_t HeightAtPos_NAV(int32_t x, int32_t z) {
    if (!(x < gFlyZoneD + 20000 && z < gFlyZoneA + 20000 && x >= gFlyZoneB - 20000 && z >= gFlyZoneC - 20000))
        return 100000;
    for (int32_t i = 1; i < 100; i++) {
        void* base = gFlyBases[i];
        if (base == nullptr) continue;
        // (v / 32) rounded toward zero: srawi then addze
        const int32_t dx = (x - FlyField(base, 0x14)) / 32;
        const int32_t dz = (z - FlyField(base, 0x1c)) / 32;
        const int32_t sum = dx * dx + dz * dz;
        const int32_t dist = static_cast<int32_t>(std::sqrt(static_cast<double>(sum)));
        if (static_cast<int32_t>(static_cast<uint32_t>(dist) << 5) < 10000) return 30000;
    }
    if (!(x < gFlyZoneD && z < gFlyZoneA && x >= gFlyZoneB && z >= gFlyZoneC)) return 0;
    int32_t cx = x / 300 / 10;
    int32_t cz = z / 300 / 10;
    if (cx < 0) cx = 0;
    if (cz < 0) cz = 0;
    if (cx >= gHeightfieldWidth) cx = gHeightfieldWidth - 1;
    if (cz >= gHeightfieldHeight) cz = gHeightfieldHeight - 1;
    const float h = GetWorldGSquareHeight(cx, cz, nullptr);
    return static_cast<int32_t>(h) * 300;
}

void GetNextHoldingWaypoint(FLYBOY* fly) {
    uint8_t* p = reinterpret_cast<uint8_t*>(fly);
    fly->mPrevX58 = fly->mX4c;
    fly->mPrevH5c = fly->mH50;
    fly->mPrevZ60 = fly->mZ54;
    void* base = gFlyBases[fly->m0];
    const int32_t limit = FlyField(base, 0x18) + 5000;
    fly->mH50 = gFlyHoldMax < limit ? limit : gFlyHoldMax;
    uint32_t* state = reinterpret_cast<uint32_t*>(p + 0x88);
    *state = (*state + 1) & 3;
    base = gFlyBases[fly->m0];
    const int32_t bx = FlyField(base, 0x14);
    const int32_t bz = FlyField(base, 0x1c);
    switch (*state) {
        case 0: fly->mX4c = bx + 25000; fly->mZ54 = bz + 25000; break;
        case 1: fly->mX4c = bx + 25000; fly->mZ54 = bz - 25000; break;
        case 2: fly->mX4c = bx - 25000; fly->mZ54 = bz - 25000; break;
        default: fly->mX4c = bx - 25000; fly->mZ54 = bz + 25000; break;
    }
}
