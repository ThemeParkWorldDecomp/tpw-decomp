// Test library for CTrackCell::Delete (unit v5990; libtpw_unit_CTrackCell_Delete.dylib, tests/test_CTrackCell_Delete.py), main app:
//   src/TrackCell_flags_CTrackCell_Delete.cpp   CTrackCell::Delete
//
// Every callee outside the unit is a fake that records the arguments the real one reads (a cell as its index in the world's track cells)
// and answers from the test's queue. The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TrackCell.h"
#include "World.h"
#include "ThingReceivers.h"
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files
int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
int32_t gLLCCE034;
int32_t gLLCCE038;
int32_t gLLCC47E0;
int32_t gLLCC47E4;
CActionRecorder gActionRecorder;
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;
extern const MapDelta gMapDeltaPlusX = {1, 0, 0};
extern const MapDelta gMapDeltaPlusY = {0, 1, 0};
extern const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};

namespace {

// A cell as its index in the world's track cells (the PowerPC fakes log the same number).
int64_t Ref(const void* cell) {
    return static_cast<int64_t>(static_cast<const CTrackCell*>(cell) - gGameWorld->mTrackCells);
}

CTrackCell* CellFromId(int64_t id) { return id == 0 ? nullptr : &gGameWorld->mTrackCells[id - 1]; }

uint8_t Rot2(uint8_t d) { return static_cast<uint8_t>((d << 2) | (d >> 6)); }
uint8_t Rot4(uint8_t d) { return static_cast<uint8_t>((d << 4) | (d >> 4)); }

}  // namespace

// ---- the fakes
uint32_t LLCC_SystemGetKartTrackCost() {
    ev("KartCost", {});
    return static_cast<uint32_t>(ret("KartCost"));
}
uint32_t LLCC_SystemGetWaterTrackCost() {
    ev("WaterCost", {});
    return static_cast<uint32_t>(ret("WaterCost"));
}
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}
uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {Ref(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}
void CCellBase::RemoveNeighbour(uint8_t mask) { ev("RemoveNeighbour", {Ref(this), mask}); }
void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {Ref(this), direction}); }
void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModifyNeighbours", {Ref(this), mask}); }
void CCellBase::RemoveDirection(uint8_t direction) { ev("RemoveDirection", {Ref(this), direction}); }
void CCellBase::SetOverlapCounter(int32_t count) { ev("SetOverlapCounter", {Ref(this), count}); }
int32_t CCellBase::NumberOfDirections() const {
    ev("NumberOfDirections", {Ref(this)});
    return static_cast<int32_t>(ret("NumberOfDirections"));
}
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Ref(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
int32_t CCellBase::NumberOfHVNeighbours() const {
    ev("NumberOfHVNeighbours", {Ref(this)});
    return static_cast<int32_t>(ret("NumberOfHVNeighbours"));
}
int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {Ref(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}
CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {Ref(this)});
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(0x6B000000u + ((Ref(this) + 1) & 0xff)));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this))});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id{};
    id.mIndex = static_cast<uint16_t>(ret("ThingId"));
    return id;
}
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return Rot4(direction);
}
uint8_t ADD_DIRECTION(uint8_t first, uint8_t second) {
    ev("ADD_DIRECTION", {first, second});
    return Rot2(first);
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
int32_t CObject::GetResidualValuePercentage() const {
    ev("GetResidualValuePercentage", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this))});
    return static_cast<int32_t>(ret("Pct"));
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(gGameWorld))});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x5A5A5A00u));
}
void CBankAccount::MakeDeposit(uint32_t amount) {
    ev("MakeDeposit", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this)), amount});
}
CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("GetNeighbouringCellDelta", {Ref(this), delta.mX, delta.mY, offset});
    return CellFromId(ret("NbrDelta"));
}
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("GetNeighbouringCellDir", {Ref(this), direction, offset});
    return CellFromId(ret("NbrDir"));
}
void CTrackCell::ModifyType(CELL_MODES mode) { ev("ModifyType", {Ref(this), static_cast<int32_t>(mode)}); }
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {Ref(this)}); }
void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag) { ev("LLCR_DeleteRide", {x, y, flag}); }

extern "C" {

uint64_t tpw_ctd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLLCCFirstBlockX", &gLLCCFirstBlockX}, {"gLLCCFirstBlockY", &gLLCCFirstBlockY},
        {"gLLCCE034", &gLLCCE034},               {"gLLCCE038", &gLLCCE038},
        {"gLLCC47E0", &gLLCC47E0},               {"gLLCC47E4", &gLLCC47E4},
        {"gRecording", &gActionRecorder.mRecording},
        {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ctd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ctd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mSegmentNumber));
    return n;
}

// gThingArray[index].mThing = value
void tpw_ctd_set_thing(int32_t index, uint64_t value) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(value)); }

void tpw_ctd_delete(uint64_t cell, int32_t a, int32_t b, int32_t c) {
    reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(cell))->Delete(a, b, c != 0);
}

}  // extern "C"
