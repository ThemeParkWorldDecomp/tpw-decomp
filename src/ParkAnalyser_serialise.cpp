// CParkAnalyser::Serialise 0x100e3004 (main app, analyser.cpp range; unit w162): 4,432 bytes, the park analyser's save-file member.
//
// CThing::Serialise (0x1012e010) of the base runs first, whatever save and version are, and a nonzero answer ends the member with 1. Then, only
// when 0 <= version < 2 (two signed compares), save == 0 loads and any other save value saves; any other version answers 0 with nothing more
// called. Each branch is written out straight, as the listing has it twice, in this order, a nonzero answer of any call ending the member with 1
// at once and the end answering 0:
//   mMapSquaresPerCell, mGridSizeX, mGridSizeY, mMapSizeX, mMapSizeY (int), the 0x400 cells, the 4 object-id histories, the six cached values,
//   the 13 + 1 + 2 + 1 + 1 integer and float histories, the 0x14 opinion histories, four counters, six time stamps, three more counters.
// The member calls get (file, save, version, unused) as received (also `save`, which is 0 in the load branch); the VAR calls get
// (file, &field, "name", 0) and never see `unused`.
//
// THE ORIGINAL'S SLIP: the load branch calls SAVEVAR<int> (0x1000f4b0), not LOADVAR, for mMisbehavingKidsEver (0x21c04; listing 0x100e3f6c).
#include <cstdint>

#include "History.h"
#include "TbTimeDiff.h"
#include "Serialise.h"
#include "ThingParts2.h"
#include "analyser.h"

int32_t CParkAnalyser::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (save == 0) {
        if (version >= 2) return 0;
        if (version < 0) return 0;
        if (LOADVAR<int32_t>(file, &mMapSquaresPerCell, "mMapSquaresPerCell", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mGridSizeX, "mGridSizeX", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mGridSizeY, "mGridSizeY", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMapSizeX, "mMapSizeX", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMapSizeY, "mMapSizeY", 0) != 0) return 1;
        for (int32_t i = 0; i < 0x400; ++i) {
            if (mCells[i].Serialise(file, save, version, unused) != 0) return 1;
        }
        for (int32_t i = 0; i < 4; ++i) {
            if (mLastOfObjectType[i].SerialiseStruct(file, save, version, unused) != 0) return 1;
        }
        if (mVal1F0B8.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0C4.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0D0.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0DC.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0E8.Serialise(file, save, version, unused) != 0) return 1;
        if (mVal1F0F4.Serialise(file, save, version, unused) != 0) return 1;
        for (int32_t i = 0; i < 13; ++i) {
            if (mHistory1F100[i].Serialise(file, save, version, unused) != 0) return 1;
        }
        if (mHistory20F10.Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory21160[0].Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory21160[1].Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory21600.Serialise(file, save, version, unused) != 0) return 1;
        if (mHistory216D8.Serialise(file, save, version, unused) != 0) return 1;
        for (int32_t i = 0; i < 0x14; ++i) {
            if (mOpinionHistory[i].Serialise(file, save, version, unused) != 0) return 1;
        }
        if (LOADVAR<int32_t>(file, &mMisbehavingKids, "mMisbehavingKids", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMisbehavingKidsThatGotAwayWithIt, "mMisbehavingKidsThatGotAwayWithIt", 0) != 0) return 1;
        // The original's load branch saves this field (SAVEVAR, not LOADVAR).
        if (SAVEVAR<int32_t>(file, &mMisbehavingKidsEver, "mMisbehavingKidsEver", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mLifetimeVisitors, "mLifetimeVisitors", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStrike), "mLastStrike", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPuke), "mLastPuke", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPrank), "mLastPrank", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mParkLastOpened), "mParkLastOpened", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastObjectBuilt), "mLastObjectBuilt", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStaffHired), "mLastStaffHired", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mMostPaidForTicket, "mMostPaidForTicket", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mLargestBusLoad, "mLargestBusLoad", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, &mLongestStay, "mLongestStay", 0) != 0) return 1;
        return 0;
    }
    if (version >= 2) return 0;
    if (version < 0) return 0;
    if (SAVEVAR<int32_t>(file, &mMapSquaresPerCell, "mMapSquaresPerCell", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mGridSizeX, "mGridSizeX", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mGridSizeY, "mGridSizeY", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMapSizeX, "mMapSizeX", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMapSizeY, "mMapSizeY", 0) != 0) return 1;
    for (int32_t i = 0; i < 0x400; ++i) {
        if (mCells[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    for (int32_t i = 0; i < 4; ++i) {
        if (mLastOfObjectType[i].SerialiseStruct(file, save, version, unused) != 0) return 1;
    }
    if (mVal1F0B8.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0C4.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0D0.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0DC.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0E8.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1F0F4.Serialise(file, save, version, unused) != 0) return 1;
    for (int32_t i = 0; i < 13; ++i) {
        if (mHistory1F100[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    if (mHistory20F10.Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory21160[0].Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory21160[1].Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory21600.Serialise(file, save, version, unused) != 0) return 1;
    if (mHistory216D8.Serialise(file, save, version, unused) != 0) return 1;
    for (int32_t i = 0; i < 0x14; ++i) {
        if (mOpinionHistory[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    if (SAVEVAR<int32_t>(file, &mMisbehavingKids, "mMisbehavingKids", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMisbehavingKidsThatGotAwayWithIt, "mMisbehavingKidsThatGotAwayWithIt", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMisbehavingKidsEver, "mMisbehavingKidsEver", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mLifetimeVisitors, "mLifetimeVisitors", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStrike), "mLastStrike", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPuke), "mLastPuke", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastPrank), "mLastPrank", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mParkLastOpened), "mParkLastOpened", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastObjectBuilt), "mLastObjectBuilt", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, reinterpret_cast<TbTimeStamp*>(mLastStaffHired), "mLastStaffHired", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mMostPaidForTicket, "mMostPaidForTicket", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mLargestBusLoad, "mLargestBusLoad", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, &mLongestStay, "mLongestStay", 0) != 0) return 1;
    return 0;
}
