// Test library for the three leaving-park states of CPerson (unit v5006; libtpw_unit_CPerson_ProcessStateWaitingToCrossRoadOutOfPark.dylib,
// tests/test_CPerson_ProcessStateWaitingToCrossRoadOutOfPark.py), main app:
//   src/game/people/PersonQueue.cpp   ProcessStateLeavingPark, ProcessStateWaitingToCrossRoadOutOfPark,
//                                                             ProcessStateWaitingForBusOuttaHere
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CWorld* gWorld;
CMap* gWorldMap;

// ---- the fakes
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetBusStopPos(int32_t which) const {
    ev("GetBusStopPos", {lab(this), which});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("GetBusStopPos"));
    return r;
}

int32_t CWorld::ArrivalVehicle_CrossingIsClear() {
    ev("ArrivalVehicle_CrossingIsClear", {lab(this)});
    return static_cast<int32_t>(ret("ArrivalVehicle_CrossingIsClear"));
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}

int32_t CWorld::ArrivalVehicle_GetStatus() {
    ev("ArrivalVehicle_GetStatus", {lab(this)});
    return static_cast<int32_t>(ret("ArrivalVehicle_GetStatus"));
}

int32_t CWorld::ArrivalVehicle_IsSeagoing() {
    ev("ArrivalVehicle_IsSeagoing", {lab(this)});
    return static_cast<int32_t>(ret("ArrivalVehicle_IsSeagoing"));
}

int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("IsInsidePark"));
}

int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}

int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDest", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
    return static_cast<int32_t>(ret("SetDest"));
}

void CPeep::SpawnParticleEffect(int32_t effect) { ev("SpawnParticleEffect", {lab(this), effect}); }

void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int64_t>(state)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return r;
}

void CThing::KillMe() { ev("KillMe", {lab(this)}); }

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

extern "C" {

uint64_t tpw_cp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gWorldMap", &gWorldMap},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cp_layout(int64_t* out) {
    int32_t n = 0;
    alignas(CMapCell) static char storage[sizeof(CMapCell)];
    CMapCell* cell = reinterpret_cast<CMapCell*>(storage);
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&cell->mWho) - storage);
    return n;
}

void tpw_cp_leaving(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateLeavingPark(); }
void tpw_cp_cross(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateWaitingToCrossRoadOutOfPark(); }
void tpw_cp_bus(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateWaitingForBusOuttaHere(); }

}  // extern "C"
