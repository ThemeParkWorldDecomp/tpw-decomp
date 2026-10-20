// RideDB.cpp (main app 0x101547b4 and 0x10154800, unit w114): the ride database's walk over its ride records.
#include "RideDB.h"
#include "TPEngine.h"

// Puts the position (mF72C) on slot 0 and answers that slot (NULL for an empty database; the count is not looked at).
RideInfo* CRideDatabase::GetFirstRide() {
    mF72C = 0;
    return mRides[mF72C];
}

// While the position is below the count less one (signed; the subtraction wraps in 32 bits) moves it on by one and answers that slot,
// else answers NULL and leaves the position where it is. A NULL slot inside the count is answered as it is. (CRideDatabase::
// DeleteCustomEffects does the same walk in line.)
RideInfo* CRideDatabase::GetNextRide() {
    if (mF72C < static_cast<int32_t>(static_cast<uint32_t>(mRideCount) - 1u)) {
        mF72C = mF72C + 1;
        return mRides[mF72C];
    }
    return nullptr;
}
