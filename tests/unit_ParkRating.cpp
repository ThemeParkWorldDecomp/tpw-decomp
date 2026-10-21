// Differential library for unit w159, the park rating, the park value and the attraction points (src/game/park/analyser.cpp).
//
// Every callee outside the unit is a fake that records the same event as the PowerPC side's stub: the world accessor, CWorld::GetUsedThingHead and
// GetNextUsedThing, GAME_ASSERTMSG, CPeep::IsInsidePark, the analyser value's Get and Set, CRideDatabase::GetRideInfo, both CControlManager::GetObjectControl
// overloads, CObject::IsCondemned, IsAvailableToChoose and GetObjectAge, CParkTime::GetFunnyTimeSinceGameTurn and the nine CMapCell predicates. The answers come
// from queues. The real sources linked besides the unit's own are the analyser readers, the object queries and Things2_small's NumberOfStaffType.
//
// The map cell predicates are NOT logged one by one (GetParkValue asks 16,384 x 9 times): each answers from a hash of the case seed, the predicate kind and
// the cell's place, and folds (kind, place, flag) into one running hash; the hash and the count are logged once, before the next logged event.
//
// The world is NOT a block of its Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and the world pointer is that block's address
// minus 0x1da6f0, on both sides (gWorld too). The map cells are never read: the predicates compute the place from the cell's address.
#define TPW_CALLLOG_EVENTS 600000
#define TPW_CALLLOG_LABELS 512
#define TPW_CALLLOG_QUEUE_VALUES 40000
#include "calllog.cpp"

#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MapCell.h"
#include "ObjectControl.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "Clock.h"
#include "History.h"
#include "tb_rain.h"

using namespace calllog;

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjId;

// only a place for the ride database (the symbol of gRideDatabase, so that the library does not need the database's translation)
alignas(16) char gRideDBBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kAnalyserSize = 0x21c48 + 2 * kGuard;
constexpr int32_t kValAt = 0x1f0e8;
constexpr int32_t kThingSlots = 8;
constexpr int32_t kRides = 5;
constexpr int32_t kControls = 3;
constexpr int32_t kRideBytes = 0x4000;

alignas(16) unsigned char gTail[kTailSize];
alignas(16) unsigned char gAnalyser[kAnalyserSize];
alignas(16) unsigned char gThings[kThingSlots][0x448];
alignas(16) unsigned char gRides[kRides][kRideBytes];   // RideInfo objects (sizeof 2824)
alignas(16) unsigned char gControls[kControls][0x20];

// the cell fakes' state: the seed and the answer thresholds of the case, the running hash and the number of calls since the last logged event
uint32_t gSeed;
uint32_t gFlagDensity;
uint32_t gDensity;
uint64_t gHash;
int64_t gCount;

constexpr uint64_t kHashStart = 0xcbf29ce484222325ull & 0x3fffffffffffffffull;

CWorld* WorldPtr() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail) - kTail); }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gAnalyser + kGuard); }
int64_t Off(const void* a, const void* b) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(a) - reinterpret_cast<intptr_t>(b)); }
CObject* Thing(int32_t slot) { return reinterpret_cast<CObject*>(gThings[slot]); }

RideInfo* RideAnswer() {
    const int64_t v = ret("rideinfo", 0);
    return v >= 1 && v <= kRides ? reinterpret_cast<RideInfo*>(gRides[v - 1]) : nullptr;
}

// Logs the pending hash of the cell predicates (once), then the event.
void Flush() {
    if (gCount > 0) {
        ev("CellHash", {static_cast<int64_t>(gHash), gCount});
        gCount = 0;
    }
}
void Ev(const char* name, std::initializer_list<int64_t> ints) {
    Flush();
    ev(name, ints);
}
void Ev(const char* name, std::initializer_list<int64_t> ints, std::initializer_list<const char*> texts) {
    Flush();
    ev(name, ints, texts);
}

// A cell predicate: the place of the cell (1 based), the answer from the hash of (seed, kind, place) and the fold of (kind, place, flag).
int32_t CellAnswer(const void* cell, uint32_t kind, uint32_t flag) {
    const uint64_t place = static_cast<uint64_t>(Off(cell, WorldPtr()) - 0x2d8) / 0x44 + 1;
    gHash = ((gHash ^ (kind | (place << 8) | (static_cast<uint64_t>(flag) << 24))) * 0x100000001b3ull) & 0x3fffffffffffffffull;
    gCount++;
    uint32_t x = static_cast<uint32_t>((place * 0x9e3779b1ull + kind * 0x85ebca6bull + gSeed * 0xc2b2ae35ull) & 0xffffffffull);
    x ^= x >> 15;
    x = static_cast<uint32_t>((x * 0x2c1b3c6dull) & 0xffffffffull);
    x ^= x >> 12;
    const uint32_t density = kind == 0 ? gFlagDensity : gDensity;
    if ((x & 15) >= density) return 0;
    static const uint32_t answers[4] = {1, 0x80000000u, 0xffffffffu, 2};
    return static_cast<int32_t>(answers[(x >> 8) & 3]);
}

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { Ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    Ev("GetWorld", {lab(this)});
    return *WorldPtr();
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    Ev("GetUsedThingHead", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head", 0));
    return result;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    Ev("GetNextUsedThing", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next", 0));
    return result;
}

int32_t CPeep::IsInsidePark() const {
    Ev("IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("inside", 0));
}

template <typename T>
T CParkAnalyserVal<T>::Get() const {
    Ev("ValGet", {lab(this)});
    return static_cast<T>(ret("get", 0));
}
template <typename T>
void CParkAnalyserVal<T>::Set(T value) {
    Ev("ValSet", {lab(this), static_cast<int64_t>(value)});
}
template int CParkAnalyserVal<int>::Get() const;
template void CParkAnalyserVal<int>::Set(int);

RideInfo* CRideDatabase::GetRideInfo(ObjId id) {
    Ev("GetRideInfo", {lab(this), id.mIndex});
    return RideAnswer();
}

// the two overloads of the object control lookup: the control comes from its own queue
ObjectControl* CControlManager::GetObjectControl(ObjId type, int32_t flag) {
    const int32_t which = static_cast<int32_t>(ret("ctl", 0)) % kControls;
    Ev("GetObjectControl", {Off(this, WorldPtr()), type.mIndex, flag, which});
    return reinterpret_cast<ObjectControl*>(gControls[which]);
}
const ObjectControl* CControlManager::GetObjectControl(ObjId type, int32_t flag) const {
    const int32_t which = static_cast<int32_t>(ret("ctlc", 0)) % kControls;
    Ev("GetObjectControlConst", {Off(this, WorldPtr()), type.mIndex, flag, which});
    return reinterpret_cast<const ObjectControl*>(gControls[which]);
}

int32_t CObject::IsCondemned() const {
    Ev("IsCondemned", {lab(this)});
    return static_cast<int32_t>(ret("cond", 0));
}
int32_t CObject::IsAvailableToChoose() {
    Ev("IsAvailableToChoose", {lab(this)});
    return static_cast<int32_t>(ret("avail", 0));
}
int32_t CObject::GetObjectAge() const {
    Ev("GetObjectAge", {lab(this)});
    return static_cast<int32_t>(ret("age", 0));
}

TbTimeDiff CParkTime::GetFunnyTimeSinceGameTurn(uint32_t turn) const {
    Ev("GetFunnyTimeSinceGameTurn", {Off(this, WorldPtr()), turn});
    TbTimeDiff span;
    span.mTicks = ret("span", 0);
    return span;
}

int32_t CMapCell::IsFlag(uint16_t flag) const { return CellAnswer(this, 0, flag); }
int32_t CMapCell::IsWater() const { return CellAnswer(this, 1, 0); }
int32_t CMapCell::IsPath() const { return CellAnswer(this, 2, 0); }
int32_t CMapCell::IsQueue() const { return CellAnswer(this, 3, 0); }
int32_t CMapCell::IsEntrance() const { return CellAnswer(this, 4, 0); }
int32_t CMapCell::IsExit() const { return CellAnswer(this, 5, 0); }
int32_t CMapCell::IsBridge() const { return CellAnswer(this, 6, 0); }
int32_t CMapCell::IsGrass() const { return CellAnswer(this, 7, 0); }
int32_t CMapCell::IsTrack() const { return CellAnswer(this, 8, 0); }

// ---- the entries of the test
extern "C" {
void* tpw_pq_addr(int32_t which) {
    switch (which) {
        case 0: return gAnalyser;
        case 1: return Analyser();
        case 2: return WorldPtr();
        case 4: return gRideDBBlock;
        case 6: return reinterpret_cast<unsigned char*>(Analyser()) + kValAt;
        case 10: return gThings[0];
        default:
            if (which >= 20 && which < 20 + kRides) return gRides[which - 20];
            if (which >= 40 && which < 40 + kControls) return gControls[which - 40];
            return gThings[which - 10];   // 11 .. 15
    }
}
void tpw_pq_reset() {
    std::memset(gAnalyser, 0xa5, sizeof gAnalyser);
    std::memset(gTail, 0xa5, sizeof gTail);
    std::memset(gThings, 0xa5, sizeof gThings);
    std::memset(gRides, 0xa5, sizeof gRides);
    std::memset(gControls, 0xa5, sizeof gControls);
    gWorld = WorldPtr();
    gHash = kHashStart;
    gCount = 0;
}
void tpw_pq_objhead(uint32_t id) { std::memcpy(gTail + 0x56, &id, 2); }
void tpw_pq_turn(uint32_t turn) { std::memcpy(gTail + 0x1c, &turn, 4); }
void tpw_pq_val(uint32_t stamp, int32_t value, uint32_t worked) {
    unsigned char* val = reinterpret_cast<unsigned char*>(Analyser()) + kValAt;
    std::memcpy(val, &stamp, 4);
    std::memcpy(val + 4, &value, 4);
    val[8] = static_cast<unsigned char>(worked);
}
void tpw_pq_cells(uint32_t seed, uint32_t flagDensity, uint32_t density) {
    gSeed = seed;
    gFlagDensity = flagDensity;
    gDensity = density;
}
void tpw_pq_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
void tpw_pq_thing(int32_t slot, int32_t kind, int32_t dead, uint32_t otype, uint32_t f0c, uint32_t f40bits, uint32_t f4c) {
    CObject* o = Thing(slot);
    reinterpret_cast<CThing*>(o)->mKind = static_cast<uint8_t>(kind);
    reinterpret_cast<CThing*>(o)->mDead = static_cast<uint8_t>(dead);
    o->mObjectType = static_cast<uint16_t>(otype);
    o->mF0C = static_cast<uint16_t>(f0c);
    std::memcpy(&o->mF40, &f40bits, 4);
    o->mF4C = static_cast<uint8_t>(f4c);
}
// a ride record, by field (the host RideInfo has its own layout): the UI type, the slots the unit reads and the 4 upgrade costs
void tpw_pq_ride(int32_t index, uint32_t uitype, uint32_t track, uint32_t decay, uint32_t value, uint32_t bonus0, uint32_t bonus1, uint32_t bonus2,
                 const uint32_t* costs) {
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRides[index]);
    ride->mUIType = static_cast<int32_t>(uitype);
    ride->Value(kRideSlot_WhichTrackType).mInteger = static_cast<int32_t>(track);
    ride->Value(kRideSlot_NewAttractionDecayTime).mInteger = static_cast<int32_t>(decay);
    ride->Value(kRideSlot_AttractionValue).mInteger = static_cast<int32_t>(value);
    const uint32_t bonus[3] = {bonus0, bonus1, bonus2};
    for (int32_t k = 0; k < 3; ++k) ride->Value(static_cast<RideBalanceSlot>(kRideSlot_Attraction_NewBonus + k)).mInteger = static_cast<int32_t>(bonus[k]);
    for (int32_t k = 0; k < 4; ++k)
        ride->Value(static_cast<RideBalanceSlot>(kRideSlot_Upgrades_CostOfUpgrade + 16 * k)).mInteger = static_cast<int32_t>(costs[k]);
}
void tpw_pq_control(int32_t index, int32_t number, uint32_t first) {
    ObjectControl* c = reinterpret_cast<ObjectControl*>(gControls[index]);
    c->mNumberInPark = number;
    c->mFirstBuiltTurn = first;
}

// fn 0: GetOverallParkRating, 1: GetParkValue(seconds), 2: GetTotalParkAttractionPoints
uint32_t tpw_pq_run(int32_t fn, uint32_t seconds) {
    CParkAnalyser* p = Analyser();
    int32_t result = 0;
    switch (fn) {
        case 0: result = p->GetOverallParkRating(); break;
        case 1: result = p->GetParkValue(seconds); break;
        default: result = p->GetTotalParkAttractionPoints(); break;
    }
    Flush();
    return static_cast<uint32_t>(result);
}
}

// ---- link-only fakes: the real sources linked besides the unit's own need these symbols; the test never reaches them (each aborts)
#include <cstdlib>
alignas(16) char gLinkClock[64] __asm__("_gClock");
alignas(16) char gLinkGameWorld[64] __asm__("_gGameWorld");
alignas(16) char gLinkSoundBanks[256] __asm__("_gSoundBanks");
alignas(16) char gLinkWorldMap[256] __asm__("_gWorldMap");
alignas(16) char gLinkRain[256] __asm__("_gei_Rain");

void SOUND_SendEvent(int, int, int, int, int, int) { std::abort(); }
void SOUND_FadeSoundEffect(int) { std::abort(); }
RideInfo* CRideDatabase::GetNextRide() { std::abort(); }
RideInfo* CRideDatabase::GetFirstRide() { std::abort(); }
void CRain::SetRainType(int) { std::abort(); }
void CRain::SetRaindrops(int) { std::abort(); }
CParkAnalyser* CWorld::GetParkAnalyser() { std::abort(); }
int32_t CParkAnalyser::GetVisitorsInPark(uint32_t) const { std::abort(); }
const CResearchLab* CWorld::GetResearchLab() const { std::abort(); }
int32_t CResearchLab::GetNumberLeftToResearch(ResearchCat) const { std::abort(); }
TbTimeStamp CParkTime::GetFunnyTime(int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*) const { std::abort(); }
int32_t CClock::ReadFixed() { std::abort(); }
template <> void CHistory<ObjId, 3>::SetAtEnd(ObjId) { std::abort(); }
template <> bool CHistory<ObjId, 3>::GetValue(ObjId&, int32_t) const { std::abort(); }
