// Differential library for unit w153, the analyser's ride superlatives (src/game/park/analyser.cpp).
//
// Every callee is a fake that records the same event as the PowerPC side's stub: GAME_ASSERTMSG (condition and text), the world accessor, CWorld::GetUsedThingHead
// and GetNextUsedThing, CRideDatabase::GetRideInfo (the object type id), CObject::GetDBInfo, GetExcitementLevel, IsCoaster, IsTrackRide, IsBOQConnected and
// GetNumPeopleInQueue (the object), BMP_CountTrackSections (the ride word and which pointers are NULL), GetCoasterTrackStats, LLCM_GetCurrentObject and the
// analyser value's Get and Set. The answers come from queues, in the order of the calls; the two ride records are shared blocks the fakes fill just before
// answering (the code reads them at once).
//
// The world is NOT a block of its Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), as in unit_ParkReaders.cpp. gWorld points at the
// same fake world (FindRideWithUnconnectedQueue reads it directly). The things are CObject sized blocks.
#include <cstdint>
#include <cstring>

#include "LLCellSystem.h"
#include "RideDB.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "Debug.h"
#include "calllog.h"

using namespace calllog;

// only the place of the ride database (every call to it is a fake)
alignas(16) char gRideDatabaseBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kAnalyserSize = 0x21c48;
constexpr int32_t kThingSlots = 8;
constexpr int32_t kResultSize = 0x30;   // a result halfword at +0x10 with 0x10 guard bytes each side

alignas(16) unsigned char gTail[kTailSize];
alignas(16) unsigned char gAnalyser[kAnalyserSize + 2 * kGuard];
alignas(16) unsigned char gResult[kResultSize];
alignas(16) unsigned char gThings[kThingSlots][sizeof(CObject)];
alignas(16) char gRideABlock[sizeof(RideInfo)];
#define gRideA (*reinterpret_cast<RideInfo*>(gRideABlock))
//   // answered by GetRideInfo (its mUIType is set from the queue "ui")
alignas(16) char gRideBBlock[sizeof(RideInfo)];
#define gRideB (*reinterpret_cast<RideInfo*>(gRideBBlock))
//   // answered by GetDBInfo (its WhichTrackType is set from the queue "track")

CWorld* WorldPtr() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail) - kTail); }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gAnalyser + kGuard); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& Result() { return *reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>*>(gResult + 0x10); }
CObject* Thing(int32_t slot) { return reinterpret_cast<CObject*>(gThings[slot]); }
float FloatOfBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
uint32_t BitsOfFloat(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;

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

// The ride record of an object type: only the id is logged; the record's mUIType comes from the queue.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    gRideA.mUIType = static_cast<int32_t>(ret("ui", 0));
    return &gRideA;
}

// The ride record of an object: the record's WhichTrackType comes from the queue.
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    gRideB.Value(kRideSlot_WhichTrackType).mInteger = static_cast<int32_t>(ret("track", 0));
    return &gRideB;
}

// The excitement level: the queue's word may carry bits above the byte (the Mac returns a register); the host's answer is the byte.
uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {lab(this), flag});
    return static_cast<uint8_t>(ret("exc", 0));
}
int32_t CObject::IsCoaster() const {
    ev("IsCoaster", {lab(this)});
    return static_cast<int32_t>(ret("coaster", 0));
}
int32_t CObject::IsTrackRide() const {
    ev("IsTrackRide", {lab(this)});
    return static_cast<int32_t>(ret("trackride", 0));
}
int32_t CObject::IsBOQConnected() {
    ev("IsBOQConnected", {lab(this)});
    return static_cast<int32_t>(ret("boq", 0));
}
int32_t CObject::GetNumPeopleInQueue() const {
    ev("GetNumPeopleInQueue", {lab(this)});
    return static_cast<int32_t>(ret("queue", 0));
}

// The track sections: the ride word and a mask of the pointers that are not NULL; each of those is written from the queue "cs"; the answer is "count".
int32_t BMP_CountTrackSections(int32_t ride, int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    const int64_t mask = (a ? 1 : 0) | (b ? 2 : 0) | (c ? 4 : 0) | (d ? 8 : 0);
    ev("BMP_CountTrackSections", {ride, mask});
    if (a) *a = static_cast<int32_t>(ret("cs", 0));
    if (b) *b = static_cast<int32_t>(ret("cs", 0));
    if (c) *c = static_cast<int32_t>(ret("cs", 0));
    if (d) *d = static_cast<int32_t>(ret("cs", 0));
    return static_cast<int32_t>(ret("count", 0));
}

// The coaster statistics: three floats (their bit patterns come from the queues "fa", "fh", "fc") and the loops.
void GetCoasterTrackStats(void* object, float* a, float* height, float* c, uint32_t* loops) {
    ev("GetCoasterTrackStats", {lab(object)});
    *a = FloatOfBits(static_cast<uint32_t>(ret("fa", 0)));
    *height = FloatOfBits(static_cast<uint32_t>(ret("fh", 0)));
    *c = FloatOfBits(static_cast<uint32_t>(ret("fc", 0)));
    *loops = static_cast<uint32_t>(ret("loops", 0));
}

// The cell system's current object: the queue "cur" holds 0 (NULL) or a slot number plus one.
CObject* LLCM_GetCurrentObject() {
    ev("LLCM_GetCurrentObject");
    const int64_t slot = ret("cur", 0);
    return slot == 0 ? nullptr : Thing(static_cast<int32_t>(slot) - 1);
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

// ---- the entries of the test
extern "C" {
// 0 the analyser, 1 the world, 2 the tail, 3 the result block (+0x10 the halfword), 10 + slot a thing, 20 the thing table, 21 the ride database
void* tpw_pg_addr(int32_t which) {
    switch (which) {
        case 0: return Analyser();
        case 1: return WorldPtr();
        case 2: return gTail;
        case 3: return gResult;
        case 4: return gAnalyser;
        case 5: return gRideDatabaseBlock;
        default: return gThings[which - 10];
    }
}
// Resets the blocks to the guard pattern.
void tpw_pg_reset() {
    std::memset(gTail, 0xa5, sizeof gTail);
    std::memset(gAnalyser, 0xa5, sizeof gAnalyser);
    std::memset(gResult, 0xa5, sizeof gResult);
    std::memset(gThings, 0xa5, sizeof gThings);
    gWorld = WorldPtr();
}
void tpw_pg_turn(uint32_t turn) { std::memcpy(gTail + 0x1c, &turn, 4); }
// Both cached values (mVal1F0C4 and mVal1F0F4) get the same time stamp, value and worked-out byte.
void tpw_pg_val(int32_t worked, uint32_t timeSig, int32_t value) {
    CParkAnalyser* a = Analyser();
    CParkAnalyserVal<int>* vals[2] = {&a->mVal1F0C4, &a->mVal1F0F4};
    for (CParkAnalyserVal<int>* v : vals) {
        v->mTimeSig = timeSig;
        v->mVal = value;
        std::memcpy(reinterpret_cast<unsigned char*>(&v->mWorkedOut), &worked, 1);
    }
}
void tpw_pg_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
void tpw_pg_thing(int32_t slot, int32_t kind, int32_t type, int32_t flags, int32_t f24) {
    CObject* o = Thing(slot);
    o->mBase.mKind = static_cast<uint8_t>(kind);
    o->mObjectType = static_cast<uint16_t>(type);
    o->mFlags = static_cast<uint16_t>(flags);
    o->mF24 = f24;
}
uint32_t tpw_pg_exciting() { return Analyser()->GetMostExcitingGokart(Result()); }
int32_t tpw_pg_crossroads() { return Analyser()->GetGokartWithMostCrossRoads(Result()); }
int32_t tpw_pg_longest_type(uint32_t type) {
    return Analyser()->GetLongestTrackRide(Result(), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(type)});
}
int32_t tpw_pg_longest_track(int32_t track) { return Analyser()->GetLongestTrackRide(Result(), track); }
uint32_t tpw_pg_loops(uint32_t type) {
    return Analyser()->GetCoasterWithMostLoops(Result(), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(type)});
}
uint32_t tpw_pg_highest(uint32_t type) {
    return Analyser()->GetHighestCoaster(Result(), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(type)});
}
uint32_t tpw_pg_toilet() { return BitsOfFloat(Analyser()->GetAverageToiletQueueLenght()); }
uint32_t tpw_pg_unconnected(int32_t any) {
    Result() = Analyser()->FindRideWithUnconnectedQueue(any != 0);
    return Result().mIndex;
}
int32_t tpw_pg_average(uint32_t seconds) { return Analyser()->GetAverageExcitementLevelOfAllRides(seconds); }
uint32_t tpw_pg_scariest() { return Analyser()->GetScariestRide(Result()); }
int32_t tpw_pg_rides(uint32_t seconds) { return Analyser()->GetRidesInPark(seconds); }
}
