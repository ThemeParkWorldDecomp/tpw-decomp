// HLSprite.cpp range of the main app (SimTheme Park 2.04b3), unit w133: FLY_SetFlyZone (0x100755a0, 180 bytes).
#include <cstdint>

#include "HLSprite.h"
#include "LLCellSystem.h"

int32_t gFlyZoneB = 0x11170;   // 0x1029d2b0
int32_t gFlyZoneA = 0x222e0;   // 0x1029d2b4
int32_t gFlyZoneD = 0x2e630;   // 0x1029d2ac
int32_t gFlyZoneC = 0x7530;    // 0x1029d2b8

// The fly zone from the hoarding extents: LLCR_GetHoardingExtents writes the four limits (to B, C, D and A, in that argument order), then
// B and C become (v * 10 + 0x23) * 300 and D and A (v * 10 - 0x23) * 300 (32-bit wrap-around).
void FLY_SetFlyZone() {
    LLCR_GetHoardingExtents(&gFlyZoneB, &gFlyZoneC, &gFlyZoneD, &gFlyZoneA);
    gFlyZoneB = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneB) * 10u + 0x23u) * 300u);
    gFlyZoneC = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneC) * 10u + 0x23u) * 300u);
    gFlyZoneD = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneD) * 10u - 0x23u) * 300u);
    gFlyZoneA = static_cast<int32_t>((static_cast<uint32_t>(gFlyZoneA) * 10u - 0x23u) * 300u);
}
