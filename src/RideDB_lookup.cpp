// CRideDatabase::GetRideInfo (main app, RideDB.cpp range; unit w200): 0x10154868, 104 bytes. The ride record of an object type.
#include <cstdint>

#include "RideDB.h"
#include "analyser.h"
#include "TPEngine.h"

// The Mac keeps the id (the HIGH halfword of its register argument) on its stack and compares the zero-extended halfwords. The cached type answers its
// cached record. Otherwise the slots are walked from 0 while the index is below the count (a signed compare, so a negative count walks nothing): the first
// record whose first halfword is the id becomes the cache and is the answer; no record answers NULL and leaves the cache as it was.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    if (id.mIndex == mCachedId) return mCachedRide;
    for (int32_t i = 0; i < mRideCount; ++i) {
        RideInfo* ride = mRides[i];
        if (ride->mId == id.mIndex) {
            mCachedId = id.mIndex;
            mCachedRide = ride;
            return ride;
        }
    }
    return nullptr;
}
