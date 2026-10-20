// Differential library for unit w151, the park analyser's object and ride type queries (src/ParkAnalyser_objects.cpp, src/Object_queries.cpp).
//
// Every callee is a fake that records the same event as the PowerPC side's stub: the world accessor (its receiver), CWorld::GetUsedThingHead and
// GetNextUsedThing, GAME_ASSERTMSG, CParkAnalyserVal<int>::Get and Set, CRideDatabase::GetFirstRide, GetNextRide and GetRideInfo, the const
// CControlManager::GetObjectControl, the const CWorld::GetResearchLab, CResearchLab::GetNumberLeftToResearch and the history's GetValue and SetAtEnd. The
// answers come from queues.
//
// The world is NOT a block of its Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and the world pointer is that block's address
// minus 0x1da6f0, on both sides. The things, the ride records and the object controls are raw blocks whose fields are set through the host's member names.
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "Debug.h"
#include "History.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"

using namespace calllog;

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjId;

// only a place for the ride database (the symbol of gRideDatabase, so that the library does not need the database's translation)
alignas(16) char gRideDBBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kAnalyserSize = 0x21c48 + 2 * kGuard;
constexpr int32_t kThingSlots = 6;
constexpr int32_t kRides = 5;
constexpr int32_t kControls = 3;

alignas(16) unsigned char gTail[kTailSize];
alignas(16) unsigned char gAnalyser[kAnalyserSize];
alignas(16) unsigned char gThings[kThingSlots][0x448];
alignas(16) unsigned char gRides[kRides][sizeof(RideInfo) + 16];
alignas(16) unsigned char gControls[kControls][0x20];
alignas(16) unsigned char gLab[0x40];

CWorld* WorldPtr() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail) - kTail); }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gAnalyser + kGuard); }
int64_t Off(const void* a, const void* b) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(a) - reinterpret_cast<intptr_t>(b)); }
CObject* Thing(int32_t slot) { return reinterpret_cast<CObject*>(gThings[slot]); }
RideInfo* Ride(int32_t index) { return reinterpret_cast<RideInfo*>(gRides[index]); }

RideInfo* RideAnswer(const char* key) {
    const int64_t v = ret(key, 0);
    return v >= 1 && v <= kRides ? Ride(static_cast<int32_t>(v) - 1) : nullptr;
}

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *WorldPtr();
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

template <typename T>
T CParkAnalyserVal<T>::Get() const {
    ev("ValGet", {lab(this)});
    return static_cast<T>(ret("get", 0));
}
template <typename T>
void CParkAnalyserVal<T>::Set(T value) {
    ev("ValSet", {lab(this), static_cast<int64_t>(value)});
}
template int CParkAnalyserVal<int>::Get() const;
template void CParkAnalyserVal<int>::Set(int);

RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {lab(this)});
    return RideAnswer("first");
}
RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {lab(this)});
    return RideAnswer("nextride");
}
RideInfo* CRideDatabase::GetRideInfo(ObjId id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return RideAnswer("rideinfo");
}

const ObjectControl* CControlManager::GetObjectControl(ObjId type, int32_t flag) const {
    const int32_t which = static_cast<int32_t>(ret("ctl", 0)) % kControls;
    ev("GetObjectControl", {Off(this, WorldPtr()), type.mIndex, flag, which});
    return reinterpret_cast<const ObjectControl*>(gControls[which]);
}

const CResearchLab* CWorld::GetResearchLab() const {
    ev("GetResearchLab", {lab(this)});
    return reinterpret_cast<const CResearchLab*>(gLab);
}

int32_t CResearchLab::GetNumberLeftToResearch(ResearchCat category) const {
    ev("GetNumberLeftToResearch", {lab(this), static_cast<int64_t>(category)});
    return static_cast<int32_t>(ret("left", 0));
}

template <class T, int N>
bool CHistory<T, N>::GetValue(T& out, int32_t back) const {
    const bool found = ret("gv", 0) != 0;
    ev("HistoryGetValue", {Off(this, Analyser()), back, found});
    out.mIndex = static_cast<uint16_t>(ret("gvid", 0));
    return found;
}
template <class T, int N>
void CHistory<T, N>::SetAtEnd(T value) {
    ev("HistorySetAtEnd", {Off(this, Analyser()), value.mIndex});
}
template bool CHistory<ObjId, 3>::GetValue(ObjId&, int32_t) const;
template void CHistory<ObjId, 3>::SetAtEnd(ObjId);

// ---- the entries of the test
extern "C" {
void* tpw_po_addr(int32_t which) {
    switch (which) {
        case 0: return gAnalyser;
        case 1: return Analyser();
        case 2: return WorldPtr();
        case 3: return gLab;
        case 4: return gRideDBBlock;
        case 5: return gTail;
        case 6: return &Analyser()->mVal1F0DC;
        case 10: return gThings[0];
        default:
            if (which >= 20 && which < 20 + kRides) return gRides[which - 20];
            if (which >= 40 && which < 40 + kControls) return gControls[which - 40];
            return gThings[which - 10];   // 11 .. 15
    }
}
int32_t tpw_po_sizes(int32_t which) { return which == 0 ? kAnalyserSize : static_cast<int32_t>(sizeof(RideInfo)); }
void tpw_po_reset() {
    std::memset(gAnalyser, 0xa5, sizeof gAnalyser);
    std::memset(gTail, 0xa5, sizeof gTail);
    std::memset(gThings, 0xa5, sizeof gThings);
    std::memset(gRides, 0xa5, sizeof gRides);
    std::memset(gControls, 0xa5, sizeof gControls);
    std::memset(gLab, 0xa5, sizeof gLab);
}
void tpw_po_objhead(uint32_t id) { std::memcpy(gTail + 0x56, &id, 2); }
void tpw_po_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
void tpw_po_thing(int32_t slot, int32_t kind, uint32_t flags, uint32_t f40bits, uint32_t f4c, uint32_t otype, uint32_t f0c, uint32_t f64, int32_t state) {
    CObject* o = Thing(slot);
    reinterpret_cast<CThing*>(o)->mKind = static_cast<uint8_t>(kind);
    o->mFlags = static_cast<uint16_t>(flags);
    std::memcpy(&o->mF40, &f40bits, 4);
    o->mF4C = static_cast<uint8_t>(f4c);
    o->mObjectType = static_cast<uint16_t>(otype);
    o->mF0C = static_cast<uint16_t>(f0c);
    o->mF64 = f64;
    o->mState = state;
}
void tpw_po_ride(int32_t index, uint32_t id, int32_t uitype, int32_t golden) {
    RideInfo* r = Ride(index);
    r->mId = static_cast<uint16_t>(id);
    r->mUIType = uitype;
    r->Value(kRideSlot_GoldenTicketCost).mInteger = golden;
}
void tpw_po_control(int32_t index, uint32_t available, int32_t upgrades, int32_t number, uint32_t first) {
    ObjectControl* c = reinterpret_cast<ObjectControl*>(gControls[index]);
    c->mAvailable = static_cast<uint8_t>(available);
    c->mUpgradesAvailable = upgrades;
    c->mNumberInPark = number;
    c->mFirstBuiltTurn = first;
}
uint32_t tpw_po_isclosed(int32_t slot) { return Thing(slot)->IsClosed(); }
uint32_t tpw_po_isbroken(int32_t slot) { return Thing(slot)->IsBrokenDown(); }

// One entry for the 26 queries (the order of the test's table); the id-returning ones answer the id, FilthiestToilet writes *filth.
uint32_t tpw_po_run(int32_t fn, int32_t a, int32_t b, int32_t* filth) {
    CParkAnalyser* p = Analyser();
    ObjId id;
    id.mIndex = static_cast<uint16_t>(a);
    const bool flag = (b & 0xff) != 0;
    switch (fn) {
        case 0: return p->NumberOfToiletsInPark();
        case 1: return p->NumberOfClosedToiletsInPark();
        case 2: return p->NumberOfStaffRoomsInPark();
        case 3: return p->NumberOfClosedStaffRoomsInPark();
        case 4: return p->AvgFilthLevelOfToilets();
        case 5: return p->FilthiestToilet(*filth).mIndex;
        case 6: return p->NumberOfBrokenDownRidesInPark();
        case 7: return p->NumberOfWornRidesInPark(a);
        case 8: return p->NumberOfObjectTypeInPark(static_cast<UIObjType>(a), b);
        case 9: return p->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(a));
        case 10: return p->GetHighestUpgradeLevelForObject(id);
        case 11: return p->TypesOfRidesAvailable((a & 0xff) != 0);
        case 12: return p->TypesOfUpgradesAvailable((a & 0xff) != 0);
        case 13: return p->TypesOfShopsAvailable((a & 0xff) != 0);
        case 14: return p->TypesOfSideshowsAvailable((a & 0xff) != 0);
        case 15: return p->TypesOfFeaturesAvailable((a & 0xff) != 0);
        case 16: return p->TypesOfRidesLeftToResearch();
        case 17: return p->TypesOfShopsLeftToResearch();
        case 18: return p->TypesOfSideshowsLeftToResearch();
        case 19: return p->TypesOfFeaturesLeftToResearch();
        case 20: return p->AllObjectsResearchedAndPresent();
        case 21: return p->PercentageOfTotalObjectsInPark();
        case 22: return p->TypesOfParticularObjectTypeInPark(static_cast<UIObjType>(a), flag);
        case 23: return p->NumberOfObjectInPark(id);
        case 24: return p->LastOfObjectTypeResearched(static_cast<UIObjType>(a), b).mIndex;
        case 25: {
            ObjId second;
            second.mIndex = static_cast<uint16_t>(b);
            p->SetLastOfObjectTypeResearched(static_cast<UIObjType>(a), second);
            return 0;
        }
        default: return 0;
    }
}
}
