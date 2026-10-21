// Test library for CPeep::SetRandomDestination (unit v5989; libtpw_unit_CPeep_SetRandomDestination.dylib, tests/test_CPeep_SetRandomDestination.py), main app:
//   src/game/people/Peep.cpp   SetRandomDestination and gPeepNoCell
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queue; cells are named by their byte
// offset from gWorldMap. The globals of other files (gWorld, gWorldMap, the four compass offsets) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "World.h"
#include "MapCell.h"
#include "CellBase.h"
#include "ObjectParts.h"
#include "Debug.h"

using namespace calllog;

CWorld* gWorld;
CMap* gWorldMap;
extern const MapDelta gMapDeltaPlusX = {1, 0, 0};
extern const MapDelta gMapDeltaPlusY = {0, 1, 0};
extern const MapDelta gMapDeltaMinusY = {0, -1, 0};
extern const MapDelta gMapDeltaMinusX = {-1, 0, 0};

namespace {

int64_t Off(const void* p) {
    return static_cast<int32_t>(static_cast<int64_t>(reinterpret_cast<intptr_t>(p)) - static_cast<int64_t>(reinterpret_cast<intptr_t>(gWorldMap)));
}

}  // namespace

// ---- the fakes
bool CPeep::IsStranded() {
    ev("IsStranded", {lab(this)});
    return ret("IsStranded") != 0;
}

bool CPeep::IsLocalAreaUnchangedSinceBecomingStranded() {
    ev("IsLocal", {lab(this)});
    return ret("IsLocal") != 0;
}

void CPeep::SetStranded(int32_t stranded) { ev("SetStranded", {lab(this), stranded}); }

int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDestTag", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("SetDestTag"));
}

int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDestMap", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
    return static_cast<int32_t>(ret("SetDestMap"));
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) { ev("ShowThoughtBubble", {lab(this), static_cast<int32_t>(type), always ? 1 : 0}); }

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

int32_t CStaff::InsidePatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) const {
    ev("InsidePatrolArea", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("InsidePatrolArea"));
}

int32_t CStaff::FindDestInPatrolArea() {
    ev("FindDestInPatrolArea", {lab(this)});
    return static_cast<int32_t>(ret("FindDestInPatrolArea"));
}

int32_t CCellBase::NumberOfHVNeighbours() const {
    ev("NumberOfHVNeighbours", {Off(this)});
    return static_cast<int32_t>(ret("NumberOfHVNeighbours"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Off(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {Off(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}

int32_t CMapCell::IsPath() const {
    ev("IsPath", {Off(this)});
    return static_cast<int32_t>(ret("IsPath"));
}

int32_t CMapCell::IsQueue() const {
    ev("IsQueue", {Off(this)});
    return static_cast<int32_t>(ret("IsQueue"));
}

int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {Off(this)});
    return static_cast<int32_t>(ret("IsEntrance"));
}

int32_t CMapCell::IsExit() const {
    ev("IsExit", {Off(this)});
    return static_cast<int32_t>(ret("IsExit"));
}

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

extern uint16_t gPeepNoCell[9];

extern "C" {

uint64_t tpw_srd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gWorldMap", &gWorldMap},
        {"noCell", &gPeepNoCell[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_srd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_srd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    return n;
}

int32_t tpw_srd_call(uint64_t peep) { return reinterpret_cast<CPeep*>(static_cast<uintptr_t>(peep))->SetRandomDestination(); }

}  // extern "C"
