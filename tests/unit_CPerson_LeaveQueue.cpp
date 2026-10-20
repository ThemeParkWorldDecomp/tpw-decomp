// Test library for the person's ticket booth walk (unit v5954; libtpw_unit_CPerson_LeaveQueue.dylib, tests/test_CPerson_LeaveQueue.py), main app:
//   src/CPerson_..._CPerson_LeaveQueue.cpp   CPerson::LeaveQueue, ProcessStatePayingAtBooth, ProcessStateWaitingForParkToOpen,
//   ProcessStateWalkingToTicketBooths, FeelingAboutEntryFee; the global gPersonTableBusSprite.
//
// The callees outside the unit are recording fakes (the world, the map, the park analyser, the bank account, the thoughts, the peep, the
// other person functions). The world block answers GetBankAccount with world + 0x100 and GetParkAnalyser with world + 0x300.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "MainBalance.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld = nullptr;
CMap* gWorldMap = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
uint16_t gPersonTable[2] = {0, 25};
CMainBalance gMainBalance;

// The balance table: its constructor runs Init, the virtuals are the key functions of the vtable.
void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }

namespace {
inline const void* Back(const void* p, std::ptrdiff_t n) { return static_cast<const char*>(p) - n; }
}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%d") != nullptr) {
        va_list ap;
        va_start(ap, format);
        int32_t a = va_arg(ap, int32_t);
        int32_t b = va_arg(ap, int32_t);
        va_end(ap);
        ev("CTRACE", {static_cast<int64_t>(level), a, b}, {format});
    } else {
        ev("CTRACE", {static_cast<int64_t>(level)}, {format});
    }
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const { return {mThingId}; }

void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) { ev("RemovePersonFromQueue", {lab(this), person.mIndex}); }

void CPerson::DismissFromQueue() { ev("DismissFromQueue", {lab(this)}); }
void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int32_t>(state)}); }
void CPerson::LowerHappiness(Amount amount) { ev("LowerHappiness", {lab(this), static_cast<int32_t>(amount)}); }
void CPerson::RaiseHappiness(Amount amount) { ev("RaiseHappiness", {lab(this), static_cast<int32_t>(amount)}); }

int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDest", {lab(this), cell.mIndex});
    return 1;
}
int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) {
    ev("ShowThoughtBubble", {lab(Back(this, 0x30)), static_cast<int32_t>(type), always ? 1 : 0});
}

void CBankAccount::PayAdmission() { ev("PayAdmission", {lab(Back(this, 0x100))}); }

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("GetTicketBoothPos", {lab(this), which});
    return {static_cast<uint16_t>(ret("GetTicketBoothPos"))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetParkEntrancePos(int32_t which) const {
    ev("GetParkEntrancePos", {lab(this), which});
    return {static_cast<uint16_t>(ret("GetParkEntrancePos"))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetCrossingParkSidePos(int32_t which) const {
    ev("GetCrossingParkSidePos", {lab(this), which});
    return {static_cast<uint16_t>(ret("GetCrossingParkSidePos"))};
}

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}
int32_t CWorld::ParkGates_GetStatus() const {
    ev("ParkGates_GetStatus", {lab(this)});
    return static_cast<int32_t>(ret("ParkGates_GetStatus"));
}
int32_t CWorld::ArrivalVehicle_IsSeagoing() {
    ev("ArrivalVehicle_IsSeagoing", {lab(this)});
    return static_cast<int32_t>(ret("ArrivalVehicle_IsSeagoing"));
}
int32_t CWorld::ArrivalVehicle_GetStatus() {
    ev("ArrivalVehicle_GetStatus", {lab(this)});
    return static_cast<int32_t>(ret("ArrivalVehicle_GetStatus"));
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(reinterpret_cast<char*>(this) + 0x100);
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(reinterpret_cast<char*>(this) + 0x300);
}

int32_t CParkAnalyser::GetTotalParkAttractionPoints() {
    ev("GetTotalParkAttractionPoints", {lab(Back(this, 0x300))});
    return static_cast<int32_t>(ret("GetTotalParkAttractionPoints"));
}
void CParkAnalyser::OfferOpinionOn(int32_t opinion, uint8_t value) { ev("OfferOpinionOn", {lab(Back(this, 0x300)), opinion, value}); }

extern "C" {

uint64_t tpw_pq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pq_layout(int64_t* out) {
    int32_t n = 0;
    using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
    out[n++] = static_cast<int64_t>(sizeof(MapTag));
    out[n++] = static_cast<int64_t>(offsetof(MapTag, mIndex));
    return n;
}

void tpw_pq_setworld(uint64_t world, uint64_t map) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
}
void tpw_pq_setthing(uint32_t index, uint64_t thing) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }
void tpw_pq_setbal(uint32_t index, uint32_t bits) { gMainBalance.mValues[index].mBits = bits; }
void tpw_pq_settable(uint32_t index, uint32_t value) {
    if (index < 2) gPersonTable[index] = static_cast<uint16_t>(value);
    else gPersonTableBusSprite = static_cast<uint16_t>(value);
}

void tpw_pq_leave(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->LeaveQueue(); }
void tpw_pq_paying(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStatePayingAtBooth(); }
void tpw_pq_waiting(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateWaitingForParkToOpen(); }
void tpw_pq_walking(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateWalkingToTicketBooths(); }
int32_t tpw_pq_feeling(uint64_t person) { return reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->FeelingAboutEntryFee(); }

}  // extern "C"
