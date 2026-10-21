// Test library for a person in a ride's queue (unit v5150; libtpw_unit_CPerson_DismissFromQueue.dylib, tests/test_CPerson_DismissFromQueue.py), main app:
//   src/game/people/PersonQueue.cpp  DismissFromQueue, EvaluateQueueEdit, FindQueueDestination, ExitRide
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). People and rides are
// named by their thing id halfword (+0), map cells by their index in the map; texts by a hash.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "Debug.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files, defined here
CThingArrayEntry gThingArray[kThingArraySize];
CMap* gWorldMap;
int32_t gSoundBanks[8];

namespace {

int64_t I(uint64_t v) { return static_cast<int64_t>(v); }

int64_t Hash(const char* text) {
    uint32_t h = 5381;
    for (; *text; ++text) h = h * 33u + static_cast<uint8_t>(*text);
    return h;
}

int64_t PersonId(const void* p) { return *reinterpret_cast<const uint16_t*>(p); }

int64_t CellIndex(const void* cell) {
    return (reinterpret_cast<const char*>(cell) - reinterpret_cast<const char*>(gWorldMap)) / 0x44;
}

int64_t Pos(MapPos p) { return (static_cast<int64_t>(p.mCell) << 8) | p.mOffset; }

}  // namespace

// ---- the fakes
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {I(mThingId)});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{mThingId};
}

void CTRACE(uint32_t level, const char* format, ...) {
    int n = 0;
    for (const char* c = format; *c; ++c)
        if (*c == '%') ++n;
    va_list ap;
    va_start(ap, format);
    if (n == 0) {
        ev("CTRACE", {I(level), Hash(format)});
    } else if (n == 1) {
        uint32_t a = va_arg(ap, uint32_t);
        ev("CTRACE", {I(level), Hash(format), I(a)});
    } else {
        uint32_t a = va_arg(ap, uint32_t);
        uint32_t b = va_arg(ap, uint32_t);
        ev("CTRACE", {I(level), Hash(format), I(a), I(b)});
    }
    va_end(ap);
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {I(static_cast<uint32_t>(condition)), Hash(format)});
}

void SCR_GetScriptPosition(int32_t handle, float*, float*, float*, int32_t*) { ev("Script", {I(static_cast<uint32_t>(handle))}); }

int32_t CObject::GetPositionInQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) const {
    ev("GetPos", {PersonId(this), I(person.mIndex)});
    return static_cast<int32_t>(ret("GetPos"));
}

void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("Remove", {PersonId(this), I(person.mIndex)});
}

int32_t CObject::ConvertQPositionToMapPos(uint32_t position, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* cell, uint8_t* x, uint8_t* y) {
    ev("ConvertQ", {PersonId(this), I(position)});
    cell->mIndex = static_cast<uint16_t>(ret("cq_cell"));
    *x = static_cast<uint8_t>(ret("cq_x"));
    *y = static_cast<uint8_t>(ret("cq_y"));
    return 0;
}

void CObject::GetStandPoint(int32_t which, MapPos* first, MapPos* second) {
    ev("Stand", {PersonId(this), I(static_cast<uint32_t>(which))});
    uint16_t a = static_cast<uint16_t>(ret("sp1"));
    uint16_t b = static_cast<uint16_t>(ret("sp2"));
    first->mCell = static_cast<uint8_t>(a >> 8);
    first->mOffset = static_cast<uint8_t>(a);
    second->mCell = static_cast<uint8_t>(b >> 8);
    second->mOffset = static_cast<uint8_t>(b);
}

void CObject::NotifyExitBlocked() { ev("Blocked", {PersonId(this)}); }

void CPeep::PlaySound(int32_t category, int32_t sound) {
    ev("PlaySound", {PersonId(this), I(static_cast<uint32_t>(category)), I(static_cast<uint32_t>(sound))});
}

int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDestPos", {PersonId(this), Pos(x), Pos(y)});
    return static_cast<int32_t>(ret("SetDestPos"));
}

int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDestCell", {PersonId(this), I(cell.mIndex)});
    return static_cast<int32_t>(ret("SetDestCell"));
}

void CPeep::Teleport(MapPos x, MapPos y) { ev("Teleport", {PersonId(this), Pos(x), Pos(y)}); }

void CPerson::LowerHappiness(Amount amount) { ev("Lower", {PersonId(this), I(static_cast<uint32_t>(amount))}); }

void CPerson::SetState(PersonState state) { ev("SetState", {PersonId(this), I(static_cast<uint32_t>(state))}); }

void CPerson::UseObject(CObject* object) { ev("UseObject", {PersonId(this), PersonId(object)}); }

void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("RecordEvent", {PersonId(reinterpret_cast<const char*>(this) - 0x30), I(static_cast<uint32_t>(type)), I(thing.mIndex)});
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) {
    ev("Bubble", {PersonId(reinterpret_cast<const char*>(this) - 0x30), I(static_cast<uint32_t>(type)), I(always ? 1 : 0)});
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {CellIndex(this)});
    return static_cast<uint8_t>(ret("dir"));
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {I(direction)});
    return static_cast<uint8_t>(ret("opp"));
}

CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    ev("Neighbour", {CellIndex(this), I(direction)});
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gWorldMap) + ret("nb") * 0x44);
}

int32_t CMapCell::IsQueue() const {
    ev("IsQueue", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsQueue"));
}

extern "C" {

uint64_t tpw_dq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thingarr", &gThingArray[0]},
        {"map", &gWorldMap},
        {"sound", &gSoundBanks[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(gSoundBanks[0]));
    return n;
}

void tpw_dq_dismiss(uint64_t person) { reinterpret_cast<CPerson*>(person)->DismissFromQueue(); }
void tpw_dq_evaluate(uint64_t person) { reinterpret_cast<CPerson*>(person)->EvaluateQueueEdit(); }
uint32_t tpw_dq_find(uint64_t person) { return static_cast<uint32_t>(reinterpret_cast<CPerson*>(person)->FindQueueDestination()); }
uint32_t tpw_dq_exit(uint64_t person) { return static_cast<uint32_t>(reinterpret_cast<CPerson*>(person)->ExitRide()); }

}  // extern "C"
