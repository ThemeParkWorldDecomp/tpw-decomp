// RideDB.cpp (main app 0x10156e34, unit w88): the ride database's custom-effect clean-up.
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "rsse.h"

// 0x10156e34 (208 bytes). The position (mF72C, GetFirstRide and GetNextRide's) starts at the first slot. Each ride found frees its four
// emitters (the non-zero words) and its sound category (when not 0), and nothing is cleared afterwards: the words stay as they were. The
// next ride is the next slot while the position is below the count less one (signed; the Mac's subtraction wraps), else the walk ends;
// a NULL slot ends it too, and the first slot is visited whatever the count is. The position and the count are read again every time.
void CRideDatabase::DeleteCustomEffects() {
    mF72C = 0;
    RideInfo* ride = mRides[mF72C];
    while (ride != nullptr) {
        for (int32_t i = 0; i < 4; ++i) {
            const uint32_t handle = ride->mEmitters[i];
            if (handle != 0) PAR_KillLibEmitter(handle);
        }
        if (ride->mSoundCategory != 0) SOUND_FreeCatagory(ride->mSoundCategory);
        if (mF72C < static_cast<int32_t>(static_cast<uint32_t>(mRideCount) - 1u)) {
            mF72C = mF72C + 1;
            ride = mRides[mF72C];
        } else {
            ride = nullptr;
        }
    }
}
