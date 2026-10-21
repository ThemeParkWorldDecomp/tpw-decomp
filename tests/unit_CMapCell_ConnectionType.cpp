// Test library for CMapCell::ConnectionType (unit v51027; libtpw_unit_CMapCell_ConnectionType.dylib, tests/test_CMapCell_ConnectionType.py), main app:
//   src/game/park/MapCell.cpp   CMapCell::ConnectionType
//
// The callees outside the unit are fakes that record what the real one reads and answer from the test's queues; cells are named in the log by
// their offset from the world block (gGameWorld). The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "ObjectParts.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "ActionRecorder.h"
#include "TPEngine.h"
#include "ThingReceivers.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gLLCCC200, gLLCCC204, gLLCCC208, gLLCCC210, gLLCCC214;
char gTrackCostFree;
int32_t gLLCCDB40;
CCellBase* gLLCCDB4C;
int32_t gLLCCDB60;
uint16_t gLLCCDB68;
int32_t gLLCCDE0B4, gLLCCDE0B8, gLLCCE0DC;
int32_t gValidTypeChangeLock, gCalcNeighboursJoinEntry;
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
CThingArrayEntry gThingArray[kThingArraySize];
CRideDatabase gRideDatabase;

namespace {

constexpr int64_t kBankOffset = 0x2d8 + 0x110000 + 0x28 * 16 + 0x400;
constexpr int64_t kRideOffset = 0x2d8 + 0x110000 + 0x28 * 16 + 0x100;

char* World() { return reinterpret_cast<char*>(gGameWorld); }
int64_t Rel(const void* p) { return static_cast<int64_t>(reinterpret_cast<const char*>(p) - World()); }
CMapCell* MapCellAt(int64_t index) { return reinterpret_cast<CMapCell*>(World() + 0x2d8 + index * 0x44); }
CTrackCell* TrackCellAt(int64_t index) { return reinterpret_cast<CTrackCell*>(World() + 0x2d8 + 0x110000 + index * 0x28); }

}  // namespace

// ---- the fakes
int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {Rel(this), flag});
    return static_cast<int32_t>(ret("IsFlag"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {Rel(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetObjectThingId"));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    return reinterpret_cast<RideInfo*>(World() + kRideOffset);
}

int32_t LLCC_SystemGetPathCost() {
    ev("GetPathCost", {});
    return static_cast<int32_t>(ret("GetPathCost"));
}

int32_t LLCC_SystemGetQueueCost() {
    ev("GetQueueCost", {});
    return static_cast<int32_t>(ret("GetQueueCost"));
}

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {Rel(this)});
    return TrackCellAt(ret("GetTrackCellEquiv"));
}

int32_t CTrackCell::IsTrackX() const {
    ev("TrackIsTrackX", {Rel(this)});
    return static_cast<int32_t>(ret("TrackIsTrackX"));
}

int32_t CTrackCell::IsTrack() const {
    ev("TrackIsTrack", {Rel(this)});
    return static_cast<int32_t>(ret("TrackIsTrack"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("TrackIsCorner", {Rel(this)});
    return static_cast<uint8_t>(ret("TrackIsCorner"));
}

uint8_t CTrackCell::IsCrossRoad() const {
    ev("TrackIsCrossRoad", {Rel(this)});
    return static_cast<uint8_t>(ret("TrackIsCrossRoad"));
}

int32_t CMapCell::ValidTypeChange(CELL_MODES mode) {
    ev("ValidTypeChange", {Rel(this), static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("ValidTypeChange"));
}

int32_t CMapCell::IsTrack() const {
    ev("MapIsTrack", {Rel(this)});
    return static_cast<int32_t>(ret("MapIsTrack"));
}

void LLCC_SystemAddToSessionCost(int32_t cost) { ev("AddToSessionCost", {cost}); }

int32_t LLCC_SystemGetSessionCost() {
    ev("GetSessionCost", {});
    return static_cast<int32_t>(ret("GetSessionCost"));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {});
    return reinterpret_cast<CBankAccount*>(World() + kBankOffset);
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {});
    return static_cast<int32_t>(ret("GetBalance"));
}

void LLCM_SetIllegalPlacement(int32_t placement) { ev("SetIllegalPlacement", {placement}); }

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {Rel(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Rel(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    ev("GetNeighbouringCell", {Rel(this), direction});
    return MapCellAt(ret("GetNeighbouringCell"));
}

extern "C" {

uint64_t tpw_cmct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"c200", &gLLCCC200},       {"c204", &gLLCCC204},   {"c208", &gLLCCC208},
        {"c210", &gLLCCC210},       {"c214", &gLLCCC214},   {"costfree", &gTrackCostFree},
        {"db40", &gLLCCDB40},       {"db4c", &gLLCCDB4C},   {"db60", &gLLCCDB60},
        {"db68", &gLLCCDB68},       {"de0b4", &gLLCCDE0B4}, {"de0b8", &gLLCCDE0B8},
        {"e0dc", &gLLCCE0DC},       {"lock", &gValidTypeChangeLock}, {"join", &gCalcNeighboursJoinEntry},
        {"gameworld", &gGameWorld}, {"thingarray", &gThingArray[0]}, {"recording", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cmct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cmct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mCellId));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    return n;
}

int32_t tpw_cmct_ct(uint64_t cell, int32_t mode, int32_t flag) {
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->ConnectionType(static_cast<CELL_MODES>(mode), flag);
}

}  // extern "C"
