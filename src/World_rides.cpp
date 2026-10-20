// World.cpp (main app 0x10140fac, unit w114): whether the ride database has the rides a park needs, and the two ride masks.
#include "World.h"

#include <cstring>

#include "RideDB.h"
#include "TPEngine.h"

uint8_t gRidesOwned[0x20];          // 0x1034cb98
uint8_t gParkRequiredRides[0x20];   // 0x1034cbb8

// Rebuilds gRidesOwned from DownloadedRideNumber (minus one: byte n >> 3, bit n & 7).
// IsPreviewRide excludes a ride when `previews` is nonzero; a missing required ride gives 0.
// The original runs the mask 1, 2, 4 .. 0x80 over each byte but tests `(mask & byte) == 1`, which only the mask 1 can satisfy: only bit 0
// of each of the 32 bytes is checked (kept; the other seven bits of each byte never matter). The walk writes byte n >> 3 of the mask
// with no bound: a ride bit above 256 writes past the 32 bytes (real ones are small). The mask keeps the last result.
int32_t CWorld::DoIHaveTheseRides(const char* required, int32_t previews) {
    std::memset(gRidesOwned, 0, 0x20);
    for (RideInfo* ride = gRideDatabase.GetFirstRide(); ride != nullptr; ride = gRideDatabase.GetNextRide()) {
        bool counts = true;
        if (ride->Value(kRideSlot_IsPreviewRide).mInteger != 0 && previews != 0) counts = false;
        if (static_cast<uint32_t>(ride->Value(kRideSlot_DownloadedRideNumber).mInteger) != 0 && counts) {
            const uint32_t bit = static_cast<uint32_t>(ride->Value(kRideSlot_DownloadedRideNumber).mInteger) - 1;
            gRidesOwned[bit >> 3] = static_cast<uint8_t>(gRidesOwned[bit >> 3] | (1 << (bit & 7)));
        }
    }
    for (uint32_t i = 0; i < 0x20; ++i) {
        for (int32_t mask = 1; mask < 0x100; mask <<= 1) {
            const int32_t needed = static_cast<int8_t>(required[i]);
            const int32_t owned = static_cast<int8_t>(gRidesOwned[i]);
            if ((mask & needed) == 1 && (mask & owned) == 0) return 0;
        }
    }
    return 1;
}
