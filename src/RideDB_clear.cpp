// CRideDatabase::ClearDatabase (main app, SimTheme Park 2.04b3, RideDB.cpp; unit w81).
#include "TPEngine.h"

// 0x101546e8 (152 bytes). The custom effects first, then every ride record in order: its strings given back, the record destroyed with
// the flag 1 (which frees it), the slot cleared. The count (signed compare) and the record pointer are read again each time round, as on
// the Mac, since DeallocStrings could change them. Then the count and the lookup cache (id and record) are reset.
void CRideDatabase::ClearDatabase() {
    DeleteCustomEffects();
    for (int32_t i = 0; i < mRideCount; ++i) {
        mRides[i]->DeallocStrings();
        delete mRides[i];
        mRides[i] = nullptr;
    }
    mRideCount = 0;
    mCachedId = 0;
    mCachedRide = nullptr;
}
