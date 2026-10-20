// GetNextWaypoint 0x100745ec (1920 bytes): moves a fly's target one 20000-unit cell on and sets the height it flies at.
#include <cstddef>
#include <cstdint>

#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "World.h"

namespace {

inline int32_t Abs32(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// Snaps to the middle of a 20000 cell (truncating divide, so a negative value lands one cell lower).
inline int32_t SnapCell(int32_t v) {
    int32_t base = (v / 20000) * 20000;
    if (v < 0) base -= 20000;
    return base + 10000;
}

// Ground height under a point, in the Mac's 300-unit steps; 0 outside the fly zone.
int32_t GroundAt(int32_t x, int32_t z) {
    if (x < gFlyZoneD && z < gFlyZoneA && x >= gFlyZoneB && z >= gFlyZoneC) {
        int32_t gx = (x / 300) / 10;
        int32_t gz = (z / 300) / 10;
        if (gx < 0) gx = 0;
        if (gz < 0) gz = 0;
        if (gx >= gHeightfieldWidth) gx = gHeightfieldWidth - 1;
        if (gz >= gHeightfieldHeight) gz = gHeightfieldHeight - 1;
        return static_cast<int32_t>(GetWorldGSquareHeight(gx, gz, nullptr)) * 300;
    }
    return 0;
}

inline int32_t Sign(int32_t d) { return d == 0 ? 0 : (d > 0 ? 1 : -1); }

}  // namespace

void GetNextWaypoint(FLYBOY* fly) {
    int32_t stepX, stepZ;
    if (Abs32(fly->mX4c - fly->mPrevX58) > Abs32(fly->mZ54 - fly->mPrevZ60)) {
        stepX = Sign(fly->mX4c - fly->mPrevX58) * 20000;
        stepZ = 0;
    } else {
        stepX = 0;
        stepZ = Sign(fly->mZ54 - fly->mPrevZ60) * 20000;
    }
    fly->mPrevX58 = fly->mX4c;
    fly->mPrevH5c = fly->mH50;
    fly->mPrevZ60 = fly->mZ54;
    int32_t limit = fly->m8 + 5000;
    const int32_t rnd = Abs32(static_cast<int32_t>(gGameWorld->GetRand()));
    if (rnd % 4 != 0 && HeightAtPos_NAV(fly->mX4c + stepX, fly->mZ54 + stepZ) < limit) {
        fly->mX4c = SnapCell(fly->mX4c);
        fly->mZ54 = SnapCell(fly->mZ54);
        fly->mX4c += stepX;
        fly->mZ54 += stepZ;
    } else {
        for (;;) {
            if (HeightAtPos_NAV(fly->mX4c + stepZ, fly->mZ54 + stepX) < limit) {
                fly->mX4c += stepZ;
                fly->mZ54 += stepX;
                break;
            }
            if (HeightAtPos_NAV(fly->mX4c - stepZ, fly->mZ54 - stepX) < limit) {
                fly->mX4c -= stepZ;
                fly->mZ54 -= stepX;
                break;
            }
            if (HeightAtPos_NAV(fly->mX4c + stepX, fly->mZ54 + stepZ) < limit) {
                fly->mX4c += stepX;
                fly->mZ54 += stepZ;
                break;
            }
            if (HeightAtPos_NAV(fly->mX4c - stepX, fly->mZ54 - stepZ) < limit) {
                fly->mX4c -= stepX;
                fly->mZ54 -= stepZ;
                break;
            }
            limit += 5000;
        }
        fly->mX4c = SnapCell(fly->mX4c);
        fly->mZ54 = SnapCell(fly->mZ54);
    }
    const int32_t target = GroundAt(fly->mX4c, fly->mZ54);
    const int32_t own = GroundAt(fly->mX4, fly->mZC);
    fly->mH50 = (target < own ? own : target) + 5000;
    if (fly->mH50 < gFlyMinHeight) fly->mH50 = gFlyMinHeight;
}
