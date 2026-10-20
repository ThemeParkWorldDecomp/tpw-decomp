// CParkAnalyser::CParkAnalyser, main app 0x100e2a4c (1144 bytes), unit w122b.
// Member construction follows the Mac arrays and direct calls; rings and padding stay unwritten.
#include <cstring>

#include "World.h"
#include "analyser.h"

namespace {
// Copy the timestamp image as words, second word first (0x100e2d3c and the five following copies).
void CopyStamp(uint32_t (&target)[2], const TbTimeStamp& stamp) {
    uint32_t words[2];
    std::memcpy(words, &stamp, sizeof words);
    target[1] = words[1];
    target[0] = words[0];
}
}

CParkAnalyser::CParkAnalyser()
    : mBase(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1}, static_cast<CThing::ThingModel>(0xd)) {
    mLastStrike[1] = 0;
    mLastStrike[0] = 0;
    mLastPuke[1] = 0;
    mLastPuke[0] = 0;
    mLastPrank[1] = 0;
    mLastPrank[0] = 0;
    mParkLastOpened[1] = 0;
    mParkLastOpened[0] = 0;
    mLastObjectBuilt[1] = 0;
    mLastObjectBuilt[0] = 0;
    mLastStaffHired[1] = 0;
    mLastStaffHired[0] = 0;
    mMapSquaresPerCell = 4;
    mMapSizeX = 0x80;
    mMapSizeY = 0x80;
    mGridSizeX = 0x20;
    mGridSizeY = 0x20;
    mMisbehavingKids = 0;
    mMisbehavingKidsThatGotAwayWithIt = 0;
    mMisbehavingKidsEver = 0;
    mLifetimeVisitors = 0;
    CopyStamp(mLastStrike, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastPuke, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastPrank, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mParkLastOpened, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastObjectBuilt, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    CopyStamp(mLastStaffHired, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    mMostPaidForTicket = 0;
    mLargestBusLoad = 0;
    mLongestStay = 0;
    Init();
}
