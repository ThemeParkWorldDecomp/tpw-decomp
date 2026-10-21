// CWorld::SetupNewLevel (World.cpp, main app 0x1013a2ac, SimTheme Park 2.04b3; unit w117) and the globals of World.cpp it needs.
#include "World.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "platform/arena.h"
#include "platform/macdoze.h"

// The two names (see World.h). __sinit_World_cpp (0x10141c14) builds the strings on the Mac; until it is translated they are
// defined here with their texts.
TbDynamicStringTemplate<char16_t> gWorldGatesName(u"gates");
TbDynamicStringTemplate<char16_t> gWorldLightsName(u"lights");

namespace {

// A thing of `size` bytes (the Mac's size: the host classes are placeholders), made with its constructor; the thing's id is the half word
// at its start. The Mac tests the allocation for NULL and still asks for the id (the host's allocation never fails, so that path is not
// reproduced, as in unit w106).
template <class T> uint16_t MakeThing(uint32_t size) {
    void* block = tpw::ArenaAlloc(size);   // the Mac's game heap: the arena (a list keeps a thing's address in 32 bits)
    T* thing = new (block) T;
    return reinterpret_cast<CThing*>(thing)->GetThingId().mIndex;
}

// The first ride whose item name is `name` (ignoring case), walking the database from its first ride; NULL when none is. _wcsicmp is not
// called for a NULL ride.
RideInfo* FindRide(const TbDynamicStringTemplate<char16_t>& name) {
    RideInfo* ride = gRideDatabase.GetFirstRide();
    while (ride != nullptr && NS_MacDoze::_wcsicmp(ride->mItemName, name.mData) != 0) ride = gRideDatabase.GetNextRide();
    return ride;
}

// The "gates" or "lights" object: a CObject (0x448 bytes) on map cell 1 (cell 0, 0) of the ride's object type; its thing id.
uint16_t MakeFeatureObject(const RideInfo* ride) {
    void* block = tpw::ArenaAlloc(0x448);
    CObject* object = new (block) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1},
                                          tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0, 0);
    return reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
}

}  // namespace

// The set-up of a new level, in the Mac's order. The answer starts true and is cleared by a failed first or second balance file (the easy
// mode file's failure only asserts). The control manager's address comes from the global gGameWorld, not from `this`. The game type word is
// read twice (the Mac re-reads it after the second file). The ten things are made in a fixed order and the world keeps their ids; the
// ids of the four halfwords at 0x1da72a to 0x1da730 are cleared.
bool CWorld::SetupNewLevel(char* levelFolder) {
    bool result = true;
    mF1DA738 = 1;
    gGameWorld->GetControlManager()->SetupInitialControls();
    gActionRecorder.ResetActionRecording();
    std::memset(gParkRequiredRides, 0, sizeof gParkRequiredRides);
    char buffer[1040];
    if (static_cast<int32_t>(TheGameType().mType) == 1) {
        if (gMainBalance.Load("data:levels:online_Standard.sam", CBalanceTable::kLoadFresh) == 0) result = false;
        std::sprintf(buffer, "%s:%s", levelFolder, "online_Standard.sam");
        if (gMainBalance.Load(buffer, CBalanceTable::kLoadOverlay) == 0) result = false;
    } else {
        const int32_t first = gMainBalance.Load("data:levels:Standard.sam", CBalanceTable::kLoadFresh);
        std::sprintf(buffer, "%s:%s", levelFolder, "Standard.sam");
        const int32_t second = gMainBalance.Load(buffer, CBalanceTable::kLoadOverlay);
        result = second != 0 && first != 0;
        if (static_cast<int32_t>(TheGameType().mType) == 2) {
            std::sprintf(buffer, "%s:%s%s", levelFolder, gTPGlobals.mF427C.mData, "Standard.sam");
            if (gMainBalance.Load(buffer, CBalanceTable::kLoadOverlay) == 0)
                GAME_ASSERTMSG(0, "There is no Easy_standard.sam file for this theme - This is not critical");
        }
    }
    mTurn = 0;
    mStartTime = static_cast<uint32_t>(std::time(nullptr));
    GetMap()->Init();
    reinterpret_cast<CMacroAI*>(this)->Init();
    mPersonCount = 0;
    mStaffHQThing = MakeThing<CStaffHQ>(0x68);
    mMechanicHQThing = MakeThing<CMechanicHQ>(0xe);
    mAdvisorThing = MakeThing<CAdvisor>(0x16f0);
    mTagSystemThing = MakeThing<CTagSystem>(0x80e);
    mParkAnalyserThing = MakeThing<CParkAnalyser>(0x21c48);
    mResearchLabThing = MakeThing<CResearchLab>(0x146c);
    mWeatherThing = MakeThing<CWeather>(0x68);
    mBankAccountThing = MakeThing<CBankAccount>(0x128);
    mUIMsgReceiverThing = MakeThing<CUIMsgReceiver>(0xc);
    mChallengeHQThing = MakeThing<CChallengeHQ>(0x3e4);
    mArrivalVehicleThing = 0;
    mBusThing = 0;
    mSeaplaneThing = 0;
    mFerryThing = 0;

    // The "gates" and "lights" features of the ride database.
    const RideInfo* gates = FindRide(gWorldGatesName);
    if (gates == nullptr) {
        GAME_ASSERTMSG(0, "Could not find the gates feature\n");
        mGatesThing = 0;
    } else {
        mGatesThing = MakeFeatureObject(gates);
    }
    const RideInfo* lights = FindRide(gWorldLightsName);
    if (lights == nullptr) {
        // (the original copies the name and hands the copy's five words to the assertion, whose text has no conversion; the host cannot
        // pass a class through "...", so the words are dropped)
        const TbDynamicStringTemplate<char16_t> copy(gWorldLightsName);
        GAME_ASSERTMSG(0, "Could not find the LIGHTSNAME WAS HERE (BR 990809) feature\n");
        mLightsThing = 0;
    } else {
        mLightsThing = MakeFeatureObject(lights);
        CTRACE(2, "Found the LIGHTSNAME WAS HERE (BR 990809) feature\n");
    }

    GetResearchLab()->Init();
    BMP_InitSystem();
    Pilot::init_behaviours();

    // The first message: event 0, to the message centre (made here when there is none).
    CMsgEvent message{EventType(0)};
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    gMessageCentre->PostMessage(message);
    mF1DA738 = 0;
    return result;
}
