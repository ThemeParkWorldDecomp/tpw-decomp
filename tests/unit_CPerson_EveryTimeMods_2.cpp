// Test library for CPerson::EveryTimeMods, UpdateTemporaryHistory, UseObject, GetTimeInPark and LookForMinorDest (unit v51016;
// libtpw_unit_CPerson_EveryTimeMods_2.dylib, tests/test_CPerson_EveryTimeMods_2.py), main app:
//   src/game/people/Person.cpp
//   (it defines the five gMinor* halfwords).
//
// The callees outside the unit are recording fakes; objects are named by their offset in the case's heap block (0xFFFFFFFF for NULL), so the
// PowerPC and host logs agree. The globals of other files the unit reads (gWorld, gWorldMap, gThingArray, gRideDatabase) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "World.h"
#include "WorldSavvy.h"
#include "TPEngine.h"
#include "RideDB.h"
#include "TPCamera.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
CRideDatabase gRideDatabase;

namespace {

uint64_t gHeap = 0;   // the case's heap block: the person at +0, the objects from +0x400, the ride records at +0x1000 and +0x1800, the analyser at +0x2400

int64_t P(const void* p) { return p == nullptr ? 0xFFFFFFFFll : static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - gHeap)); }
int64_t U(uint32_t v) { return static_cast<int64_t>(v); }
int64_t S(int32_t v) { return static_cast<int64_t>(v); }

// The offset of a text in the Mac's pool at 0x102442be (the test compares the PowerPC side's pointer minus the pool base).
int64_t TextOffset(const char* text) {
    if (std::strcmp(text, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n") == 0) return 0;
    if (std::strcmp(text, "Minor Decision: OID=%d, @=(%d, %d), sc=%d\n") == 0) return 0x4f4;
    if (std::strcmp(text, "Person %d: Happiness changed by %d since using object %d\n") == 0) return 0x51f;
    return -1;
}

}  // namespace

// ---- the fakes
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {P(this)});
    return reinterpret_cast<RideInfo*>(gHeap + 0x1000);
}

void CObject::HadCustomer() { ev("HadCustomer", {P(this)}); }

void CObject::HadAWinner() { ev("HadAWinner", {P(this)}); }

void CObject::ExpressSatisfaction(int32_t value) { ev("ExpressSatisfaction", {P(this), S(value)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetBackOfQueue() {
    ev("GetBackOfQueue", {P(this)});
    return tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(ret("GetBackOfQueue"))};
}

int32_t CObject::IsAvailableToChoose() {
    ev("IsAvailableToChoose", {P(this)});
    return static_cast<int32_t>(ret("IsAvailableToChoose"));
}

uint32_t CPerson::CalculateOptionScore(CObject* object) {
    ev("CalculateOptionScore", {P(this), P(object)});
    return static_cast<uint32_t>(ret("CalculateOptionScore"));
}

void CPerson::PayPricePerUseForObject(CObject* object) { ev("PayPricePerUseForObject", {P(this), P(object)}); }

void CPerson::WinningMods(CObject* object) { ev("WinningMods", {P(this), P(object)}); }

void CPerson::LosingMods(CObject* object) { ev("LosingMods", {P(this), P(object)}); }

void CPerson::AddRideToRecentHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) { ev("AddRideToRecentHistory", {P(this), U(ride.mIndex)}); }

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {this == gWorld ? 1 : 0});
    return reinterpret_cast<CParkAnalyser*>(gHeap + 0x2400);
}

void CParkAnalyser::OfferOpinionOn(int32_t opinion, uint8_t value) { ev("OfferOpinionOn", {P(this), S(opinion), U(value)}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {this == &gRideDatabase ? 1 : 0, U(id.mIndex)});
    return reinterpret_cast<RideInfo*>(gHeap + 0x1800);
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) { ev("ShowThoughtBubble", {P(this), S(static_cast<int32_t>(type)), U(always ? 1 : 0)}); }

void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("RecordEvent", {P(this), S(static_cast<int32_t>(type)), U(thing.mIndex)});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {P(this)});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(ret("GetThingId"))};
}

CWorld& CWorldSavvy::GetWorld() const { return *gWorld; }

int32_t MAP_count_walking_distance(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> from, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> to, NavMode mode) {
    ev("MAP_count_walking_distance", {U(from.mIndex), U(to.mIndex), S(static_cast<int32_t>(mode))});
    return static_cast<int32_t>(ret("MAP_count_walking_distance"));
}

int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDest", {P(this), U(cell.mIndex)});
    return static_cast<int32_t>(ret("SetDest"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t offset = TextOffset(format);
    va_list ap;
    va_start(ap, format);
    if (offset == 0x51f) {
        int32_t a = va_arg(ap, int32_t), b = va_arg(ap, int32_t), c = va_arg(ap, int32_t);
        ev("CTRACE", {U(level), offset, S(a), S(b), S(c)});
    } else if (offset == 0x4f4) {
        int32_t a = va_arg(ap, int32_t), b = va_arg(ap, int32_t), c = va_arg(ap, int32_t), d = va_arg(ap, int32_t);
        ev("CTRACE", {U(level), offset, S(a), S(b), S(c), S(d)});
    } else {
        ev("CTRACE", {U(level), offset});
    }
    va_end(ap);
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {S(condition), TextOffset(format)}); }

extern "C" {

uint64_t tpw_cem2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},          {"gWorldMap", &gWorldMap},       {"gThingArray", gThingArray},    {"NoRide", &gMinorNoRide},
        {"NoQueue", &gMinorNoQueue},  {"ListEnd", &gMinorListEnd},     {"NoDest", &gMinorNoDest},       {"NoDest2", &gMinorNoDest2},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cem2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_cem2_setheap(uint64_t heap) { gHeap = heap; }

int32_t tpw_cem2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_cem2_every(uint64_t person, uint64_t object) { reinterpret_cast<CPerson*>(person)->EveryTimeMods(reinterpret_cast<CObject*>(object)); }
void tpw_cem2_uth(uint64_t person) { reinterpret_cast<CPerson*>(person)->UpdateTemporaryHistory(); }
void tpw_cem2_use(uint64_t person, uint64_t object) { reinterpret_cast<CPerson*>(person)->UseObject(reinterpret_cast<CObject*>(object)); }
uint32_t tpw_cem2_gtip(uint64_t person) { return reinterpret_cast<CPerson*>(person)->GetTimeInPark(); }
void tpw_cem2_lfmd(uint64_t person) { reinterpret_cast<CPerson*>(person)->LookForMinorDest(); }

}  // extern "C"
