// Test library for CPerson::ModelState, OnAdmittance, GetDebagged and MakeMajorDecision (unit v5543; libtpw_unit_CPerson_ModelState.dylib,
// tests/test_CPerson_ModelState.py), main app:
//   src/CPerson_..._CPerson_ModelState.cpp   the four functions and the globals gMajorListEnd, gMajorNoQueue, gMajorNoRide, gModelListEnd
//
// The callees outside the unit are recording fakes (same event names and arguments as the module's PowerPC stubs); the globals of other files
// the unit reads are defined here. The person, four objects, the world and the map are static blocks (the labels 1 to 7 of the log).
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "World.h"
#include "TPSound.h"
#include "analyser.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gSoundBanks[8];
uint16_t gNoAdmitState;

namespace {

alignas(16) uint8_t gPersonBlock[0x240];
alignas(16) uint8_t gObjectBlock[4][0x240];
alignas(16) uint8_t gWorldBlock[0x1da748 + 16];
alignas(16) uint8_t gMapBlock[0x100];

inline int64_t Lb(const void* p) { return lab(const_cast<void*>(p)); }
inline int64_t Pos(MapPos p) { return (static_cast<int64_t>(p.mCell) << 8) | p.mOffset; }

}  // namespace

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%d") != nullptr) {
        va_list ap;
        va_start(ap, format);
        uint32_t a = va_arg(ap, uint32_t);
        uint32_t b = va_arg(ap, uint32_t);
        va_end(ap);
        ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(a), static_cast<int64_t>(b)}, {format});
    } else {
        ev("CTRACE", {static_cast<int64_t>(level)}, {format});
    }
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {static_cast<int64_t>(condition)}, {format}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Lb(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return t;
}
void CThing::KillMe() { ev("KillMe", {Lb(this)}); }

int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {Lb(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}
int32_t CPeep::SetRandomDestination() {
    ev("SetRandomDestination", {Lb(this)});
    return static_cast<int32_t>(ret("SetRandomDestination"));
}
int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDest", {Lb(this), static_cast<int64_t>(cell.mIndex)});
    return static_cast<int32_t>(ret("SetDest"));
}
int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDestXY", {Lb(this), Pos(x), Pos(y)});
    return 0;
}
void CPeep::Teleport(MapPos x, MapPos y) { ev("Teleport", {Lb(this), Pos(x), Pos(y)}); }
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("PlaySound", {Lb(this), category, sound}); }

void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("SpawnSpriteScript", {static_cast<int64_t>(reinterpret_cast<uint8_t*>(this) - gPersonBlock), static_cast<int64_t>(script), set, index});
}

int32_t CWorld::ArrivalVehicle_CrossingIsClear() {
    ev("CrossingIsClear", {Lb(this)});
    return static_cast<int32_t>(ret("CrossingIsClear"));
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {Lb(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {Lb(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}
int32_t CWorld::ParkGates_GetStatus() const {
    ev("ParkGates_GetStatus", {Lb(this)});
    return static_cast<int32_t>(ret("ParkGates_GetStatus"));
}
int32_t CWorld::GetNewPersonIndex() {
    ev("GetNewPersonIndex", {Lb(this)});
    return static_cast<int32_t>(ret("GetNewPersonIndex"));
}
int32_t CWorld::ArrivalVehicle_IsSeagoing() {
    ev("IsSeagoing", {Lb(this)});
    return static_cast<int32_t>(ret("IsSeagoing"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("GetTicketBoothPos", {Lb(this), which});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetTicketBoothPos"));
    return t;
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetCrossingParkSidePos(int32_t which) const {
    ev("GetCrossingParkSidePos", {Lb(this), which});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetCrossingParkSidePos"));
    return t;
}

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetBackOfQueue() {
    ev("GetBackOfQueue", {Lb(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetBackOfQueue"));
    return t;
}
int32_t CObject::IsAvailableToChoose() {
    ev("IsAvailableToChoose", {Lb(this)});
    return static_cast<int32_t>(ret("IsAvailableToChoose"));
}
int32_t CObject::FinishedAdmitting() {
    ev("FinishedAdmitting", {Lb(this)});
    return static_cast<int32_t>(ret("FinishedAdmitting"));
}
void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("RemovePersonFromQueue", {Lb(this), static_cast<int64_t>(person.mIndex)});
}
bool CGuard::IsCarrying() const {
    ev("IsCarrying", {Lb(this)});
    return ret("IsCarrying") != 0;
}

void CPerson::SetState(PersonState state) { ev("SetState", {Lb(this), static_cast<int64_t>(static_cast<int32_t>(state))}); }
void CPerson::ProcessStateWalkingToTicketBooths() { ev("WalkingToTicketBooths", {Lb(this)}); }
void CPerson::ProcessStatePayingAtBooth() { ev("PayingAtBooth", {Lb(this)}); }
void CPerson::ProcessStateWaitingForParkToOpen() { ev("WaitingForParkToOpen", {Lb(this)}); }
void CPerson::ProcessStateWalksToLitterBin() { ev("WalksToLitterBin", {Lb(this)}); }
void CPerson::ProcessStateIdle() { ev("Idle", {Lb(this)}); }
void CPerson::ProcessStateWalksToMajor() { ev("WalksToMajor", {Lb(this)}); }
void CPerson::ProcessStateStandingInQueue() { ev("StandingInQueue", {Lb(this)}); }
void CPerson::ProcessStateBeingLoaded() { ev("BeingLoaded", {Lb(this)}); }
void CPerson::ProcessStateBeingUnloaded() { ev("BeingUnloaded", {Lb(this)}); }
void CPerson::ProcessStateLeavingPark() { ev("LeavingPark", {Lb(this)}); }
void CPerson::ProcessStateWaitingToCrossRoadOutOfPark() { ev("WaitingToCross", {Lb(this)}); }
void CPerson::ProcessStateWaitingForBusOuttaHere() { ev("WaitingForBus", {Lb(this)}); }
void CPerson::EndSpotAnim() { ev("EndSpotAnim", {Lb(this)}); }
uint32_t CPerson::CalculateOptionScore(CObject* object) {
    ev("CalculateOptionScore", {Lb(this), Lb(object)});
    return static_cast<uint32_t>(ret("CalculateOptionScore"));
}

extern "C" {

uint64_t tpw_ms_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"person", gPersonBlock},         {"obj0", gObjectBlock[0]},     {"obj1", gObjectBlock[1]},        {"obj2", gObjectBlock[2]},
        {"obj3", gObjectBlock[3]},        {"world", gWorldBlock},        {"map", gMapBlock},               {"gWorld", &gWorld},
        {"gWorldMap", &gWorldMap},        {"arr", &gThingArray[0]},      {"arr1", &gThingArray[1]},        {"banks", &gSoundBanks[0]},
        {"noadmit", &gNoAdmitState},      {"majorEnd", &gMajorListEnd},  {"majorNoQ", &gMajorNoQueue},     {"majorNoR", &gMajorNoRide},
        {"modelEnd", &gModelListEnd},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

int32_t tpw_ms_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA744));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    return n;
}

void tpw_ms_model() { reinterpret_cast<CPerson*>(gPersonBlock)->ModelState(); }
void tpw_ms_admit() { reinterpret_cast<CPerson*>(gPersonBlock)->OnAdmittance(); }
void tpw_ms_debag(uint32_t cell) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(cell);
    reinterpret_cast<CPerson*>(gPersonBlock)->GetDebagged(t);
}
void tpw_ms_major() { reinterpret_cast<CPerson*>(gPersonBlock)->MakeMajorDecision(); }

}  // extern "C"
