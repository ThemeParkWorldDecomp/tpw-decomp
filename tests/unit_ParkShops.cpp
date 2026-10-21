// Differential library for unit w152, the park analyser's shop and sideshow superlatives (src/game/park/analyser.cpp): the twelve
// CParkAnalyser::GetShopWith... and GetSideshowWith... queries.
//
// Every callee is a fake that records the same event as the PowerPC side's stub: GAME_ASSERTMSG (the condition and the text), the world accessor (its
// receiver), CWorld::GetUsedThingHead and GetNextUsedThing, CObject::GetDBInfo, GetObjectAge and GetCustomerSatisfaction (the object), CHistory<unsigned
// int,30>::GetSumOfLast (the object, the history's offset and the count) and CRideDatabase::GetRideInfo (the object type id). The list order comes from
// queues; the ages, satisfactions and sums are tables per thing (so that the two reads of one sum agree), the ride blocks a table per object type.
//
// Memory: the analyser is the middle of a 0xc0 byte block (0x40 guard bytes each side), the result halfword the middle of a 0x30 byte block; the world
// is a block nothing reads. The things are 0x448 byte blocks (CObject's size) in one array, the ride records are RideInfo blocks, one per object type.
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"

using namespace calllog;

// only a place for the ride database (the symbol of gRideDatabase, so that the library does not need its constructor)
alignas(16) char gRideDatabaseBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

constexpr int32_t kSlots = 6;
constexpr int32_t kRides = 4;
constexpr int32_t kObjectSize = 0x448;
constexpr int32_t kAnalyserOffset = 0x40;
constexpr int32_t kAnalyserBlock = 0xc0;
constexpr int32_t kResultOffset = 0x10;
constexpr int32_t kResultBlock = 0x30;

alignas(16) unsigned char gThings[kSlots][kObjectSize];
alignas(16) unsigned char gRides[kRides][sizeof(RideInfo)];
alignas(16) unsigned char gAnalyser[kAnalyserBlock];
alignas(16) unsigned char gResult[kResultBlock];
alignas(16) unsigned char gWorldBlock[0x100];

// What the fakes answer for a thing.
struct ThingAnswers {
    int32_t age;
    int32_t satisfaction;
    double sum[2];   // the last 30 of mHistory1A4 and of mHistory22C
};
ThingAnswers gAnswers[kSlots];

CWorld* WorldPtr() { return reinterpret_cast<CWorld*>(gWorldBlock); }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gAnalyser + kAnalyserOffset); }
RideInfo* Ride(int32_t type) { return reinterpret_cast<RideInfo*>(gRides[type]); }

// The slot of an object (or of a pointer inside one) and the offset in it.
int32_t SlotOf(const void* p) { return static_cast<int32_t>((reinterpret_cast<const unsigned char*>(p) - &gThings[0][0]) / kObjectSize); }
int32_t OffsetOf(const void* p) { return static_cast<int32_t>((reinterpret_cast<const unsigned char*>(p) - &gThings[0][0]) % kObjectSize); }

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

// The ride record of the object's type (the case builds one block per type).
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return Ride(mObjectType);
}

int32_t CObject::GetObjectAge() const {
    ev("GetObjectAge", {lab(this)});
    return gAnswers[SlotOf(this)].age;
}

int32_t CObject::GetCustomerSatisfaction() const {
    ev("GetCustomerSatisfaction", {lab(this)});
    return gAnswers[SlotOf(this)].satisfaction;
}

// The sum of a history of a thing: the log names the thing, the history's offset (0x1a4 or 0x22c) and the count.
template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    const int32_t offset = OffsetOf(this);
    const unsigned char* object = reinterpret_cast<const unsigned char*>(this) - offset;
    ev("GetSumOfLast", {lab(object), offset, count});
    sum = gAnswers[SlotOf(this)].sum[offset == 0x1a4 ? 0 : 1];
    return count;
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    return Ride(id.mIndex);
}

// ---- the entries of the test
extern "C" {
// 0 the analyser, 1 the world, 2 the result halfword, 3 the analyser block, 4 the result block, 10 + slot a thing, 20 + type a ride record.
void* tpw_sh_addr(int32_t which) {
    switch (which) {
        case 0: return gAnalyser + kAnalyserOffset;
        case 1: return gWorldBlock;
        case 2: return gResult + kResultOffset;
        case 3: return gAnalyser;
        case 4: return gResult;
        default: return which >= 20 ? static_cast<void*>(gRides[which - 20]) : static_cast<void*>(gThings[which - 10]);
    }
}
// The memory of a case: guards 0xa5 everywhere, the result halfword set to `result`, no rows in the thing table.
void tpw_sh_reset(uint32_t result) {
    std::memset(gAnalyser, 0xa5, sizeof gAnalyser);
    std::memset(gResult, 0xa5, sizeof gResult);
    std::memset(gWorldBlock, 0xa5, sizeof gWorldBlock);
    std::memset(gThings, 0xa5, sizeof gThings);
    std::memset(gRides, 0xa5, sizeof gRides);
    std::memset(gAnswers, 0, sizeof gAnswers);
    std::memset(gThingArray, 0, sizeof gThingArray);
    const uint16_t half = static_cast<uint16_t>(result);
    std::memcpy(gResult + kResultOffset, &half, 2);
}
void tpw_sh_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
void tpw_sh_thing(int32_t slot, int32_t kind, int32_t type, int32_t age, int32_t satisfaction) {
    CObject* object = reinterpret_cast<CObject*>(gThings[slot]);
    object->mBase.mKind = static_cast<uint8_t>(kind);
    object->mObjectType = static_cast<uint16_t>(type);
    gAnswers[slot].age = age;
    gAnswers[slot].satisfaction = satisfaction;
}
void tpw_sh_sum(int32_t slot, int32_t which, double sum) { gAnswers[slot].sum[which] = sum; }
void tpw_sh_ride(int32_t type, int32_t shopType, int32_t uiType) {
    RideInfo* ride = Ride(type);
    ride->Value(kRideSlot_ShopType).mInteger = shopType;
    ride->mUIType = uiType;
}
// The twelve queries by their order in the manifest; 0 to 3 take the shop type.
int32_t tpw_sh_call(int32_t which, int32_t type) {
    const CParkAnalyser* a = Analyser();
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id = *reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>*>(gResult + kResultOffset);
    const CObject::ShopType shop = static_cast<CObject::ShopType>(type);
    switch (which) {
        case 0: return a->GetShopWithBestValue(shop, id);
        case 1: return a->GetShopWithWorstValue(shop, id);
        case 2: return a->GetShopWithBestSatisfaction(shop, id);
        case 3: return a->GetShopWithWorstSatisfaction(shop, id);
        case 4: return a->GetShopWithBestValue(id);
        case 5: return a->GetShopWithWorstValue(id);
        case 6: return a->GetShopWithBestSatisfaction(id);
        case 7: return a->GetShopWithWorstSatisfaction(id);
        case 8: return a->GetSideshowWithBestValue(id);
        case 9: return a->GetSideshowWithWorstValue(id);
        case 10: return a->GetSideshowWithBestSatisfaction(id);
        default: return a->GetSideshowWithWorstSatisfaction(id);
    }
}
}
