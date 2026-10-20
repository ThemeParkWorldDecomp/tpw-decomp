// Test library for CPerson::ProcessStateIdle (unit v5098; libtpw_unit_CPerson_ProcessStateIdle.dylib, tests/test_CPerson_ProcessStateIdle.py), main app:
//   src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_ProcessStateIdle.cpp   ProcessStateIdle and the globals it defines
//
// Every callee outside the unit is a recording fake with the PowerPC stub's event name and arguments. The globals of other files the unit reads are
// defined here (the world, the map pointer, the thing array, the message centre, the sound banks, the main balance).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

// ---- globals of other files, defined here
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
CMessageCentre* gMessageCentre;
int32_t gSoundBanks[8];
CMainBalance gMainBalance;
void CBalanceTable::Init() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return nullptr; }
CMainBalance::~CMainBalance() {}   // the program's is in src/MainBalance.cpp, which this library does not compile

namespace {
void* gAnalyserSlot = nullptr;   // what GetParkAnalyser answers
}

CSubscriptionHeader::CSubscriptionHeader() {}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- the fakes
uint32_t CWorld::GetRand() {
    ev("rand", {});
    return static_cast<uint32_t>(ret("rand"));
}
int32_t CWorld::IsParkClosed() const {
    ev("closed", {});
    return static_cast<int32_t>(ret("closed"));
}
int32_t CWorld::ParkGates_GetStatus() const {
    ev("gates", {});
    return static_cast<int32_t>(ret("gates"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("analyser", {});
    return static_cast<CParkAnalyser*>(gAnalyserSlot);
}
void CMapCell::PlaceLitter(LitterType type) {
    int64_t index = (reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gWorldMap)) / 0x44;
    ev("litter", {index, static_cast<int32_t>(type)});
}
void CThoughts::RecordEvent(EventType type, ThingId thing) {
    ev("event", {lab(reinterpret_cast<const char*>(this) - 0x30), static_cast<int32_t>(type), thing.mIndex});
}
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("sound", {lab(this), category, sound}); }
int32_t CPeep::SetDest(MapId cell) {
    ev("setdest", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("setdest"));
}
int32_t CPeep::SetRandomDestination() {
    ev("randdest", {lab(this)});
    return static_cast<int32_t>(ret("randdest"));
}
void CPerson::SetState(PersonState state) { ev("state", {lab(this), static_cast<int32_t>(state)}); }
void CPerson::LowerHappiness(Amount amount) { ev("lower", {lab(this), static_cast<int32_t>(amount)}); }
void CPerson::SetSpotAnim(CDelayedScript::ScriptIdx script) { ev("spot", {lab(this), static_cast<int32_t>(script)}); }
void CPerson::MakeBalloonPop() { ev("pop", {lab(this)}); }
void CPerson::MakeMajorDecision() {
    ev("decide", {lab(this)});
    *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(this) + 0x1dc) = static_cast<uint16_t>(ret("decide"));
}
ThingId CPerson::FindNearestReachableLitterBin(uint32_t range) {
    ev("bin", {lab(this), range});
    return ThingId{static_cast<uint16_t>(ret("bin"))};
}
ThingId CPerson::FindNearestFireworks() {
    ev("fw", {lab(this)});
    return ThingId{static_cast<uint16_t>(ret("fw"))};
}
ThingId CPerson::FindNearestEntertainer() {
    ev("ent", {lab(this)});
    return ThingId{static_cast<uint16_t>(ret("ent"))};
}
MapId CMap::GetCrossingParkSidePos(int32_t which) const {
    ev("side", {which});
    return MapId{static_cast<uint16_t>(ret("side", which, 0))};
}
int32_t CParkAnalyser::IsNearbyThingOfType(CThing::ThingModel model, MapId id, int32_t a) {
    ev("near", {lab(this), static_cast<int32_t>(model), id.mIndex, a});
    return static_cast<int32_t>(ret("near"));
}
ThingId CThing::GetThingId() const {
    ev("thingid", {lab(this)});
    return ThingId{*reinterpret_cast<const uint16_t*>(this)};
}
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const CMsgPersonIdle& m = static_cast<const CMsgPersonIdle&>(message);
    ev("post", {lab(this), m.mFlags, m.mThingId, m.mValue});
    return 1;
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t sum = 0;
    for (const char* c = format; *c; ++c) sum += static_cast<uint8_t>(*c);
    ev("assert", {condition & 0xff, sum});
}
int32_t LbMaths_ArcTan(int32_t x, int32_t y) {
    ev("arctan", {x, y});
    return static_cast<int32_t>(ret("arctan"));
}

extern "C" {

uint64_t tpw_idle_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gWorldMap", &gWorldMap},
        {"centre", &gMessageCentre},
        {"banks1", &gSoundBanks[1]},
        {"balance", &gMainBalance.mValues[38].mInteger},
        {"kinds", &gLitterKindCount},
        {"noBin", &gNoLitterBin},
        {"noFw", &gNoFireworks},
        {"noBalloon", &gNoBalloonThing},
        {"noDecision", &gNoDecision},
        {"slot", &gAnalyserSlot},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_idle_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The world, made once (zeroed, 32-bit addressable); gWorld and gWorldMap are set by the test's build as on the Mac.
uint64_t tpw_idle_world() {
    static void* world = nullptr;
    if (world == nullptr) world = tpw::ArenaAllocZeroed(sizeof(CWorld));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(world));
}

void tpw_idle_setthing(int32_t index, uint64_t pointer) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer)); }

int32_t tpw_idle_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    return n;
}

void tpw_idle_run(uint64_t person) {
    reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateIdle();
}

}  // extern "C"
