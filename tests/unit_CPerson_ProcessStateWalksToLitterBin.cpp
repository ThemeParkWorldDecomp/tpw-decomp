// Test library for three states of the person (unit v5529; libtpw_unit_CPerson_ProcessStateWalksToLitterBin.dylib,
// tests/test_CPerson_ProcessStateWalksToLitterBin.py), main app:
//   src/game/people/PersonQueue.cpp
//   ProcessStateWalksToLitterBin, ProcessStateStandingInQueue, ProcessStateWaitingForScriptToAdmitMe; defines gNoLitterRide, gNoAdmitState.
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The globals of
// other files the unit reads (gWorld, gWorldMap, gThingArray, gLitterKindCount) are defined here, zero at start like the Mac's bss.
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
#include "MapCell.h"
#include "rsse.h"
#include "TPCoaster.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld = nullptr;
CMap* gWorldMap = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
uint32_t gLitterKindCount = 6;

namespace {

uint8_t* gDbInfo = nullptr;

int CountConversions(const char* format) {
    int n = 0;
    for (const char* p = format; *p; ++p)
        if (*p == '%') ++n;
    return n > 2 ? 2 : n;
}

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int n = CountConversions(format);
    va_list ap;
    va_start(ap, format);
    int64_t a0 = 0, a1 = 0;
    if (n > 0) a0 = va_arg(ap, uint32_t);
    if (n > 1) a1 = va_arg(ap, uint32_t);
    va_end(ap);
    int64_t cond = condition & 0xff;
    if (n == 0) ev("Assert", {cond}, {format});
    else if (n == 1) ev("Assert", {cond, a0}, {format});
    else ev("Assert", {cond, a0, a1}, {format});
}

void CTRACE(uint32_t level, const char* format, ...) {
    int n = CountConversions(format);
    va_list ap;
    va_start(ap, format);
    int64_t a0 = 0, a1 = 0;
    if (n > 0) a0 = va_arg(ap, uint32_t);
    if (n > 1) a1 = va_arg(ap, uint32_t);
    va_end(ap);
    int64_t lv = level;
    if (n == 0) ev("CTRACE", {lv}, {format});
    else if (n == 1) ev("CTRACE", {lv, a0}, {format});
    else ev("CTRACE", {lv, a0, a1}, {format});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    int64_t label = lab(this);
    ev("GetThingId", {label});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(0x1230 + 0x11 * label);
    return id;
}

int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}

int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDest", {lab(this), static_cast<int64_t>((x.mCell << 8) | x.mOffset), static_cast<int64_t>((y.mCell << 8) | y.mOffset)});
    return static_cast<int32_t>(ret("SetDest"));
}

void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int64_t>(state)}); }
void CPerson::LeaveQueue() { ev("LeaveQueue", {lab(this)}); }
int32_t CPerson::FindQueueDestination() {
    ev("FindQueueDestination", {lab(this)});
    return static_cast<int32_t>(ret("FindQueueDestination"));
}
void CPerson::SetSpotAnim(CDelayedScript::ScriptIdx script) { ev("SetSpotAnim", {lab(this), static_cast<int64_t>(script)}); }

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

void CMapCell::PlaceLitter(LitterType type) {
    ev("PlaceLitter", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this))), static_cast<int64_t>(static_cast<uint32_t>(type))});
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("GetScript", {static_cast<int64_t>(script)});
    return reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(0x70001234));
}
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) {
    ev("Setvar", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(script))), index, value});
}

bool CObject::ReadyToAdmit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("ReadyToAdmit", {lab(this), person.mIndex});
    return ret("ReadyToAdmit") != 0;
}
void CObject::GetStandPoint(int32_t which, MapPos* first, MapPos* second) {
    ev("GetStandPoint", {lab(this), which});
    uint32_t v = static_cast<uint32_t>(ret("StandPoint"));
    first->mCell = static_cast<uint8_t>(v >> 24);
    first->mOffset = static_cast<uint8_t>(v >> 16);
    second->mCell = static_cast<uint8_t>(v >> 8);
    second->mOffset = static_cast<uint8_t>(v);
}
void CObject::PersonLeftDuringAdmittance(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("PersonLeftDuringAdmittance", {lab(this), person.mIndex});
}
int32_t CObject::IsDirty() const {
    ev("IsDirty", {lab(this)});
    return static_cast<int32_t>(ret("IsDirty"));
}
int32_t CObject::GetPositionInQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) const {
    ev("GetPositionInQueue", {lab(this), person.mIndex});
    return static_cast<int32_t>(ret("GetPositionInQueue"));
}
bool CObject::IsBrokenDown() const {
    ev("IsBrokenDown", {lab(this)});
    return ret("IsBrokenDown") != 0;
}
uint32_t CObject::GetIdealQueueLength() const {
    ev("GetIdealQueueLength", {lab(this)});
    return static_cast<uint32_t>(ret("GetIdealQueueLength"));
}
int32_t CObject::IsCoaster() const {
    ev("IsCoaster", {lab(this)});
    return static_cast<int32_t>(ret("IsCoaster"));
}
int32_t IsCoasterUseable(CObject* object) {
    ev("IsCoasterUseable", {lab(object)});
    return static_cast<int32_t>(ret("IsCoasterUseable"));
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return reinterpret_cast<RideInfo*>(gDbInfo);
}
int32_t CObject::FinishedAdmitting() {
    ev("FinishedAdmitting", {lab(this)});
    return static_cast<int32_t>(ret("FinishedAdmitting"));
}
void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("RemovePersonFromQueue", {lab(this), person.mIndex});
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) {
    ev("ShowThoughtBubble", {lab(this), static_cast<int64_t>(type), static_cast<int64_t>(always)});
}
void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("RecordEvent", {lab(this), static_cast<int64_t>(type), thing.mIndex});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_u5529_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"kinds", &gLitterKindCount},
        {"ffc", &gNoLitterRide},
        {"ffe", &gNoAdmitState},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_u5529_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The pointer globals: the world (its base, so that base + 0x1da70c is the turn word), the map base and the ride record the fake answers.
void tpw_u5529_set(uint64_t world, uint64_t map, uint64_t db) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
    gDbInfo = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(db));
}

void tpw_u5529_thing(uint32_t index, uint64_t address) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

int32_t tpw_u5529_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    return n;
}

void tpw_u5529_walk(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateWalksToLitterBin(); }
void tpw_u5529_queue(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateStandingInQueue(); }
void tpw_u5529_script(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->ProcessStateWaitingForScriptToAdmitMe(); }

}  // extern "C"
