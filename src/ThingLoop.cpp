// The seven thing-loop entry points (unit w123a, main app). External dispatch,
// death-message support, random/season providers and tree insertion remain callees.
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"

// Source-local Darwin ABI declarations, keeping w123b's World/ParkTime headers intact.
uint32_t ThingLoopWorldRand(CWorld*) __asm__("__ZN6CWorld7GetRandEv");   // 0x1013b1e4
void ThingLoopWorldPending(CWorld*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>)
    __asm__("__ZN6CWorld22AddToPendingDeleteListE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE");   // 0x1013b154
int32_t ThingLoopSeason(const CParkTime*) __asm__("__ZNK9CParkTime9GetSeasonEv");   // 0x101129e8

namespace {
CMessageCentre* Centre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
void SubscribeThing(CThing& thing, int32_t type) {
    Centre()->Subscribe(thing, static_cast<MsgType>(type));
}
uint32_t SeasonValue(int32_t season, int32_t slot) {
    return static_cast<uint32_t>(gMainBalance.mValues[slot + kMainBalanceSeasonStride * season].mInteger);
}
int32_t Season() { return ThingLoopSeason(&gGameWorld->mParkTime); }
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
}

// 0x1012e230, 536 bytes. The kind is unsigned and dispatch is direct, without a dead test.
void CThing::Process() {
    switch (mKind) {
    case 1:
        reinterpret_cast<CPerson*>(this)->UpdateTurn();
        reinterpret_cast<CPerson*>(this)->ModelState();
        break;
    case 3:
        reinterpret_cast<CObject*>(this)->UpdateTurn();
        reinterpret_cast<CObject*>(this)->ModelState();
        break;
    case 4:
        reinterpret_cast<CMechanic*>(this)->UpdateTurn();
        reinterpret_cast<CMechanic*>(this)->ModelState();
        break;
    case 5:
        reinterpret_cast<CHandyman*>(this)->UpdateTurn();
        reinterpret_cast<CHandyman*>(this)->ModelState();
        break;
    case 6:
        reinterpret_cast<CEntertainer*>(this)->UpdateTurn();
        reinterpret_cast<CEntertainer*>(this)->ModelState();
        break;
    case 7:
        reinterpret_cast<CGuard*>(this)->UpdateTurn();
        reinterpret_cast<CGuard*>(this)->ModelState();
        break;
    case 8:
        reinterpret_cast<CResearcher*>(this)->UpdateTurn();
        reinterpret_cast<CResearcher*>(this)->ModelState();
        break;
    case 11:
        if (TheGameType().mType != kGameTypeOnline) reinterpret_cast<CAdvisor*>(this)->UpdateTurn();
        break;
    case 13:
        reinterpret_cast<CParkAnalyser*>(this)->UpdateTurn();
        break;
    case 15:
        reinterpret_cast<CWeather*>(this)->UpdateTurn();
        break;
    case 18:
        if (TheGameType().mType == kGameTypeOnline) {
            reinterpret_cast<COnlinePerson*>(this)->UpdateTurn();
            reinterpret_cast<COnlinePerson*>(this)->ModelState();
        } else {
            CTRACE(3, "We probably shouldn't be encountering MODEL_ONLINE_PERSONs in the normal world tick should we... (?)\n");
        }
        break;
    case 19:
        if (TheGameType().mType != kGameTypeOnline) reinterpret_cast<CChallengeHQ*>(this)->UpdateTurn();
        break;
    default:
        break;
    }
}

// 0x1012e728, 568 bytes. Post first, dispatch the freshly read kind, mark dead,
// queue the freshly read id, then set kind 2. Even assertion paths reach the tail.
void CThing::KillMe() {
    CMsgDeath message({mThingId});
    void* deathVptr;
    std::memcpy(&deathVptr, static_cast<const void*>(&message), sizeof deathVptr);
    Centre()->PostMessage(message);
    // The Mac's inlined nondeleting destructor repeats the death vptr store.
    // Restore the native vptr too, without a destructor call or a Mac address.
    std::memcpy(static_cast<void*>(&message), &deathVptr, sizeof deathVptr);
    switch (mKind) {
    case 1: reinterpret_cast<CPerson*>(this)->FinalDestroy(); break;
    case 3: reinterpret_cast<CObject*>(this)->FinalDestroy(); break;
    case 4: reinterpret_cast<CMechanic*>(this)->FinalDestroy(); break;
    case 5: reinterpret_cast<CHandyman*>(this)->FinalDestroy(); break;
    case 6: reinterpret_cast<CEntertainer*>(this)->FinalDestroy(); break;
    case 7: reinterpret_cast<CGuard*>(this)->FinalDestroy(); break;
    case 8: reinterpret_cast<CResearcher*>(this)->FinalDestroy(); break;
    case 9: reinterpret_cast<CStaffHQ*>(this)->FinalDestroy(); break;
    case 10: reinterpret_cast<CMechanicHQ*>(this)->FinalDestroy(); break;
    case 11: reinterpret_cast<CAdvisor*>(this)->FinalDestroy(); break;
    case 12: reinterpret_cast<CTagSystem*>(this)->FinalDestroy(); break;
    case 13: reinterpret_cast<CParkAnalyser*>(this)->FinalDestroy(); break;
    case 14: reinterpret_cast<CResearchLab*>(this)->FinalDestroy(); break;
    case 15: reinterpret_cast<CWeather*>(this)->FinalDestroy(); break;
    case 16: reinterpret_cast<CBankAccount*>(this)->FinalDestroy(); break;
    case 17: reinterpret_cast<CUIMsgReceiver*>(this)->FinalDestroy(); break;
    case 18: reinterpret_cast<COnlinePerson*>(this)->FinalDestroy(); break;
    case 19: reinterpret_cast<CChallengeHQ*>(this)->FinalDestroy(); break;
    case 2: GAME_ASSERTMSG(0, "Trying to delete an already-deleted thing\n"); break;
    default: GAME_ASSERTMSG(0, "Dunno how to delete this CThing\n"); break;
    }
    mDead = 1;
    ThingLoopWorldPending(gWorld, {mThingId});
    mKind = 2;
}

// 0x10102d34, 200 bytes. The old head is cached across the second assertion,
// while the next link uses a later head read. Self-insertion preserves that order.
void CMapCell::InsertThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CThing* const thing = static_cast<CThing*>(gThingArray[id.mIndex].mThing);
    if (mWho != gThingLoopEmptyCellId) {
        const uint16_t oldHead = mWho;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        static_cast<CThing*>(gThingArray[oldHead].mThing)->mCellPrev = id.mIndex;
    }
    thing->mCellNext = mWho;
    thing->mCellPrev = 0;
    mWho = id.mIndex;
}

// 0x10152388, 128 bytes. The signed bound has no lower limit. Original insert_one
// produces a 0/1 byte, independently of its iterator pointer.
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    if (type >= 29) return false;
    const CSubscription key{thing.GetThingId().mIndex};
    return mSubscriptions[type].mTree.insert_one(key).mInserted;
}

// 0x10138e70, 352 bytes. Integer-only unsigned remainders and wrapping results;
// the signed tolerance test and all separate season/world reads are retained.
uint32_t CWeather::FindNewQuality() const {
    const uint32_t chance = SeasonValue(Season(), kMainBalanceExceptionalWeatherChance);
    if (ThingLoopWorldRand(gGameWorld) % 100u < chance) return ThingLoopWorldRand(gGameWorld) % 100u;
    const uint32_t tolerance = SeasonValue(Season(), kMainBalanceNormalTolerance);
    if (static_cast<int32_t>(tolerance) <= 0) return SeasonValue(Season(), kMainBalanceAvgWeatherQuality);
    const uint32_t denominator = SeasonValue(Season(), kMainBalanceNormalTolerance) << 1;
    const uint32_t remainder = ThingLoopWorldRand(gGameWorld) % denominator;
    const uint32_t average = SeasonValue(Season(), kMainBalanceAvgWeatherQuality);
    const uint32_t subtractedTolerance = SeasonValue(Season(), kMainBalanceNormalTolerance);
    return average - subtractedTolerance + remainder;
}

// 0x1000a13c, 664 bytes. Each subscription repeats the lazy-centre test.
int32_t CAdvisor::Init() {
    for (int32_t type : {5, 17, 19, 23, 14, 8, 25}) SubscribeThing(mBase, type);
    const uint32_t zero = 0;
    std::memcpy(mF0C + 4, &zero, 4);
    std::memcpy(mFD4, &zero, 4);
    mPlayDuration = 0;
    mPlayStartTime = 0;
    return 1;
}

// 0x1012c6b8, 556 bytes. The body remains untouched.
void CTagSystem::Init() {
    for (int32_t type : {7, 12, 20, 24, 25, 19}) SubscribeThing(mBase, type);
}
