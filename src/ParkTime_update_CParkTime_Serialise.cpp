// CParkTime::Serialise (0x10111fe0, 516 bytes): the park time to or from a save file. 0 on success, 1 on the first failure. The version must be 0 or 1,
// otherwise nothing is done and the answer is 0 (the same for save and load). The last argument is unused.
#include "ParkTime.h"
#include "Serialise.h"

int32_t CParkTime::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    // the names are "mFunnyTimeStart", "mSessionStart", "mMonthAtLastUpdate", "mDayAtLastUpdate", "mFunnySecsPerRealSec"
    TbTimeStamp* funny = FunnyTimeStart();
    TbTimeStamp* session = SessionStart();
    MacULong* rate = reinterpret_cast<MacULong*>(&mFunnySecsPerRealSec);
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVARSTRUCT<TbTimeStamp>(file, funny, "mFunnyTimeStart", 0) != 0) return 1;
            if (SAVEVARSTRUCT<TbTimeStamp>(file, session, "mSessionStart", 0) != 0) return 1;
            if (SAVEVAR<int>(file, &mMonthAtLastUpdate, "mMonthAtLastUpdate", 0) != 0) return 1;
            if (SAVEVAR<int>(file, &mDayAtLastUpdate, "mDayAtLastUpdate", 0) != 0) return 1;
            if (SAVEVAR<MacULong>(file, rate, "mFunnySecsPerRealSec", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVARSTRUCT<TbTimeStamp>(file, funny, "mFunnyTimeStart", 0) != 0) return 1;
            if (LOADVARSTRUCT<TbTimeStamp>(file, session, "mSessionStart", 0) != 0) return 1;
            if (LOADVAR<int>(file, &mMonthAtLastUpdate, "mMonthAtLastUpdate", 0) != 0) return 1;
            if (LOADVAR<int>(file, &mDayAtLastUpdate, "mDayAtLastUpdate", 0) != 0) return 1;
            if (LOADVAR<MacULong>(file, rate, "mFunnySecsPerRealSec", 0) != 0) return 1;
        }
    }
    return 0;
}
