// Differential library for unit w130: the things' second batch (CResearchLab::Init, SetupProjects, SetupCategories in src/ResearchLab_init.cpp;
// CChallengeHQ::AddChallenge, ClearInvalidFollowups, IsSystemOn, OfferNextChallenge in src/ChallengeHQ_offer.cpp; CParkAnalyser::BuiltSomething,
// NumberOfStaffType, CBankAccount::MakeWithdrawal, CAdvisor::IsPlaying, FindHighestScoringSlotInQueue and CWeather::SetPrecipitationType in
// src/Things2_small.cpp).
//
// Every callee outside those is a fake that records the same event as the PowerPC side's stub. The worlds are not blocks of their Mac size: only the
// 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and a world pointer is that block's address minus 0x1da6f0 on both sides. The object under
// test lives in gObj over a guard of 0x40 bytes on each side, the two analysers in gAnalyser, so a stray store shows. Objects are reported as call-log
// labels (see test_Things2.py).
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Clock.h"
#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "TbTimeDiff.h"
#include "ThingParts.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "tb_rain.h"

using namespace calllog;

// Only a place for the storage of the Mac's two singletons (their symbols; the unit never constructs them).
alignas(16) unsigned char gRideDbBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");
alignas(16) unsigned char gClockBlock[sizeof(CClock)] __asm__("_gClock");

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kObjAt = 0x40;
constexpr int32_t kObjBlock = kObjAt + 0x1700 + 0x40;
constexpr int32_t kAnalyserSize = 0x21c48;
constexpr int32_t kAnalyserBlock = kObjAt + kAnalyserSize + 0x40;
constexpr int32_t kRides = 6;
constexpr int32_t kRideBlock = 0x1000;
constexpr int32_t kThingSlots = 8;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gObj[kObjBlock];
alignas(16) unsigned char gAnalyser[2][kAnalyserBlock];
alignas(16) unsigned char gCentre[0x1d0];
alignas(16) unsigned char gRain[0x100];
alignas(16) unsigned char gChal[kObjAt + 0x30 + 0x40];
alignas(16) unsigned char gRide[kRides][kRideBlock];
alignas(16) unsigned char gThings[kThingSlots][0x40];
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gRideCount, gRidePos;
int32_t gGameTypeBuilt;

static_assert(sizeof(RideInfo) <= kRideBlock, "the ride blocks are big enough");

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }
CParkAnalyser* AnalyserPtr(int32_t k) { return reinterpret_cast<CParkAnalyser*>(gAnalyser[k] + kObjAt); }
unsigned char* Obj() { return gObj + kObjAt; }

}  // namespace

// ---- The unit's globals ---------------------------------------------------------------------------------------------------------------
CWorld* gWorld;
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
CRain* gei_Rain;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];

void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}

// ---- The worlds, the park analyser, the clock ------------------------------------------------------------------------------------------
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld;
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head", 0));
    return result;
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next", 0));
    return result;
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return AnalyserPtr(this == WorldPtr(0) ? 0 : 1);
}
bool CParkAnalyser::GatesOpen() const {
    ev("GatesOpen", {lab(this)});
    return (ret("gates") & 0xff) != 0;
}
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g)}, {});
    ev("GetFunnyTime.8", {lab(h)});
    return TbTimeStamp{ret("ft")};
}
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("SinceStart", {lab(this)});
    TbTimeDiff diff;
    diff.mTicks = ret("span");
    return diff;
}
int32_t CClock::ReadFixed() {
    ev("ReadFixed", {lab(this)});
    return static_cast<int32_t>(ret("clock"));
}

// ---- The game type: a function-local static guarded by a byte on the Mac ------------------------------------------------------------------
CGameType::CGameType() {
    ev("CGameType", {});
    gGameTypeBuilt = 1;
    mType = static_cast<GameType>(ret("gtctor"));
}

// ---- The research lab's callees -------------------------------------------------------------------------------------------------------
uint32_t CResearchLab::SetEffort(int32_t group, uint32_t effort) {
    ev("SetEffort", {lab(this), group, effort});
    return 0;
}
void CResearchLab::UpdateHighestGroup(int32_t group) { ev("UpdateHighestGroup", {lab(this), group}); }
void CResearchLab::CalculateOutputs() { ev("CalculateOutputs", {lab(this)}); }
char CResearchProject::SetupProject(const RideInfo* ride) {
    ev("SetupProject", {lab(this), lab(ride)});
    const uint32_t score = static_cast<uint32_t>(ret("pscore"));
    std::memcpy(&mScore, &score, 4);
    mCategory = static_cast<int32_t>(ret("pcat"));
    mF08 = static_cast<uint8_t>(ret("pf08"));
    return static_cast<char>(ret("sp"));
}
char CResearchProject::SetupStatUpgrade(const RideInfo* ride) {
    ev("SetupStatUpgrade", {lab(this), lab(ride)});
    const uint32_t score = static_cast<uint32_t>(ret("uscore"));
    std::memcpy(&mScore, &score, 4);
    mCategory = static_cast<int32_t>(ret("ucat"));
    mF08 = static_cast<uint8_t>(ret("uf08"));
    return static_cast<char>(ret("su"));
}
bool CResearchProject::IsProjectCompleted() const {
    ev("IsProjectCompleted", {lab(this)});
    return (ret("done") & 0xff) != 0;
}
RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {lab(this)});
    gRidePos = 0;
    return gRideCount > 0 ? reinterpret_cast<RideInfo*>(gRide[0]) : nullptr;
}
RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {lab(this)});
    ++gRidePos;
    return gRidePos < gRideCount ? reinterpret_cast<RideInfo*>(gRide[gRidePos]) : nullptr;
}

// ---- The challenge hall's callee, the trace --------------------------------------------------------------------------------------------
bool CChallengeHQ::TellPlayerChallengeOffered(CChallenge* challenge, bool force) {
    ev("Tell", {lab(this), lab(challenge), force});
    return (ret("tell") & 0xff) != 0;
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int);
    va_end(args);
    ev("CTRACE", {level, value}, {format});
}

// ---- The weather's callees --------------------------------------------------------------------------------------------------------------
void CRain::SetRainType(int32_t type) { ev("SetRainType", {lab(this), type}); }
void CRain::SetRaindrops(int32_t count) { ev("SetRaindrops", {lab(this), count}); }
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("handle"));
}
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }

// ---- The message centre ------------------------------------------------------------------------------------------------------------------
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), static_cast<int64_t>(type)});
    return ret("sub") != 0;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- The test entry points ---------------------------------------------------------------------------------------------------------------
extern "C" {

// 0, 1 the world tails; 2 the object block (the object at +0x40); 3, 4 the analyser blocks (the object at +0x40); 5 the centre block; 6 the rain
// block; 7 the challenge argument block (the object at +0x40); 8 + k a ride block; 14 + k a thing block; 22 the clock; 23 the ride database.
void* tpw_t2_addr(int32_t which) {
    if (which == 0 || which == 1) return gTail[which];
    if (which >= 8 && which < 8 + kRides) return gRide[which - 8];
    if (which >= 14 && which < 14 + kThingSlots) return gThings[which - 14];
    switch (which) {
    case 2: return gObj;
    case 3: return gAnalyser[0];
    case 4: return gAnalyser[1];
    case 5: return gCentre;
    case 6: return gRain;
    case 7: return gChal;
    case 22: return gClockBlock;
    case 23: return gRideDbBlock;
    default: return nullptr;
    }
}
void* tpw_t2_world(int32_t k) { return WorldPtr(k); }

// The globals before a case: the worlds named by index, the centre block (0 for none), the rain block, the first sound bank, the number of rides;
// the allocations of the last case are freed.
void tpw_t2_prepare(int32_t world, int32_t game, int32_t centre, int32_t rain, int32_t bank, int32_t rides) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gWorld = WorldPtr(world);
    gGameWorld = WorldPtr(game);
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gCentre) : nullptr;
    gei_Rain = rain != 0 ? reinterpret_cast<CRain*>(gRain) : nullptr;
    gSoundBanks[0] = bank;
    gRideCount = rides;
    gRidePos = 0;
}
void tpw_t2_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
void tpw_t2_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
void tpw_t2_thing(int32_t slot, int32_t kind, int32_t dead) {
    CThing* t = reinterpret_cast<CThing*>(gThings[slot]);
    t->mKind = static_cast<uint8_t>(kind);
    t->mDead = static_cast<uint8_t>(dead);
}
// The game type object, for a case where the Mac's guard byte is already set (the first use builds it with the fake constructor otherwise).
void tpw_t2_gametype(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_t2_gametype_built() { return gGameTypeBuilt; }
// The host offsets of the ride record's two members the research lab reads (0 mUIType, 1 mLoadFailed).
int32_t tpw_t2_ride_offset(int32_t which) {
    return which == 0 ? static_cast<int32_t>(__builtin_offsetof(RideInfo, mUIType)) : static_cast<int32_t>(__builtin_offsetof(RideInfo, mLoadFailed));
}
// What is left after a call: the label of the centre global and the allocations.
void tpw_t2_globals(int64_t* out) {
    out[0] = lab(gMessageCentre);
    out[1] = gAllocatedCount;
}

// which: 0 Init, 1 SetupProjects, 2 SetupCategories, 3 BuiltSomething, 4 NumberOfStaffType, 5 MakeWithdrawal, 6 AddChallenge, 7 ClearInvalidFollowups,
// 8 IsSystemOn, 9 OfferNextChallenge, 10 IsPlaying, 11 FindHighestScoringSlotInQueue, 12 SetPrecipitationType. arg: the argument (the analyser's index
// for 3 and 4 is which2).
int64_t tpw_t2_run(int32_t which, int64_t arg, int32_t which2) {
    switch (which) {
    case 0: reinterpret_cast<CResearchLab*>(Obj())->Init(); return 0;
    case 1: reinterpret_cast<CResearchLab*>(Obj())->SetupProjects(); return 0;
    case 2: reinterpret_cast<CResearchLab*>(Obj())->SetupCategories(); return 0;
    case 3: AnalyserPtr(which2)->BuiltSomething(); return 0;
    case 4: return AnalyserPtr(which2)->NumberOfStaffType(static_cast<CThing::ThingModel>(arg));
    case 5: reinterpret_cast<CBankAccount*>(Obj())->MakeWithdrawal(static_cast<uint32_t>(arg)); return 0;
    case 6: reinterpret_cast<CChallengeHQ*>(Obj())->AddChallenge(*reinterpret_cast<CChallenge*>(gChal + kObjAt)); return 0;
    case 7: reinterpret_cast<CChallengeHQ*>(Obj())->ClearInvalidFollowups(); return 0;
    case 8: return reinterpret_cast<CChallengeHQ*>(Obj())->IsSystemOn() ? 1 : 0;
    case 9: reinterpret_cast<CChallengeHQ*>(Obj())->OfferNextChallenge(arg != 0); return 0;
    case 10: return reinterpret_cast<CAdvisor*>(Obj())->IsPlaying() ? 1 : 0;
    case 11: return reinterpret_cast<CAdvisor*>(Obj())->FindHighestScoringSlotInQueue();
    default: reinterpret_cast<CWeather*>(Obj())->SetPrecipitationType(static_cast<PrecipitationType>(arg)); return 0;
    }
}

}  // extern "C"
