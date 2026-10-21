// Test library for LLCR_PlaceRide (unit v5635; libtpw_unit_LLCR_PlaceRide.dylib, tests/test_LLCR_PlaceRide.py), main app:
//   src/game/park/LLCellRide.cpp   LLCR_PlaceRide, gLLCRPlacement, gLLCRPlaceRotation, gLLCRPlaceY, gLLCRPlaceX
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queues: cells
// as their byte offset from the map grid (world + 0x2d8), track cells from the track grid (world + 0x2d8 + 0x110000), NULL as
// kNull. ADD_DIRECTION and OPPOSITE compute what the real ones do (the loops over the directions end on their answer); IsXYValid
// answers 1 inside the test's small map (128 by kRows); the ride cell kinds and directions come from the case's tables
// (tpw_pr_set_grid). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
uint8_t gLLCCE0CC;
int32_t gLLCCC214;
int32_t gLLCCDB6C;
CObject* gLLCMCurrentObject;
int32_t gLLCCE0DC;
uint16_t gLLCCE0E0;
uint8_t gLLCC47C4;
int32_t gSoundBanks[8];
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
CRideDatabase gRideDatabase;

namespace {

constexpr int64_t kNull = -1000000;
constexpr int32_t kRows = 6;
void* gTestRide;       // what LLCR_AddRideToDB answers
RideInfo* gTestInfo;   // what GetRideInfo answers
// The footprint's squares (kGrid by kGrid, row major): the kinds LLCR_GetRideCell answers and the directions
// LLCR_GetRideCellDirection answers; 0 outside.
constexpr int32_t kGrid = 8;
int32_t gTestKinds[kGrid * kGrid];
int32_t gTestDirs[kGrid * kGrid];

int32_t GridAt(const int32_t* grid, int32_t col, int32_t row) {
    if (col < 0 || col >= kGrid || row < 0 || row >= kGrid) return 0;
    return grid[row * kGrid + col];
}

uintptr_t CellBase() { return reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8; }

int64_t Cell(const void* p) {
    if (p == nullptr) return kNull;
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(p) - static_cast<intptr_t>(CellBase()));
}

int64_t Track(const void* p) {
    if (p == nullptr) return kNull;
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(p) - static_cast<intptr_t>(CellBase() + 0x110000));
}

}  // namespace

// ---- the fakes
void LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) { ev("SetRide", {id.mIndex}); }

void LLCR_GetWidthHeight(int32_t* width, int32_t* height) {
    ev("GetWH", {});
    *width = static_cast<int32_t>(ret("W"));
    *height = static_cast<int32_t>(ret("H"));
}

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return (x >= 0 && x < 128 && y >= 0 && y < kRows) ? 1 : 0;
}

int32_t LLCR_GetRideCell(int32_t col, int32_t row) {
    ev("GRC", {col, row});
    return GridAt(gTestKinds, col, row);
}

uint8_t LLCR_GetRideCellDirection(int32_t col, int32_t row) {
    ev("GRD", {col, row});
    return static_cast<uint8_t>(GridAt(gTestDirs, col, row));
}

int32_t CMapCell::Modify(CELL_MODES mode, int32_t a, int32_t b) {
    ev("Modify", {Cell(this), static_cast<int32_t>(mode), a, b});
    return static_cast<int32_t>(ret("Modify"));
}

void LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("SetTopLeft", {cell.mIndex}); }

CObject* LLCR_AddRideToDB(int32_t rotation) {
    ev("AddRide", {rotation});
    return static_cast<CObject*>(gTestRide);
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("Sound", {entity, bank, event, a, b, c});
    return 0;
}

void LLCM_SetCurrentObject(bool flag) { ev("SetCurObj", {flag ? 1 : 0}); }

void CMapCell::Delete(int32_t a, int32_t b) { ev("Delete", {Cell(this), a, b}); }

void CMapCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) { ev("ParentID", {Cell(this), id.mIndex}); }

void CTrackCell::DeleteMesh() { ev("DeleteMesh", {Track(this)}); }

void CTrackCell::UnSetFlag(uint16_t flags) { ev("TUnSetFlag", {Track(this), flags}); }

void CTrackCell::SetFlag(uint16_t flags) { ev("TSetFlag", {Track(this), flags}); }

void CMapCell::SetFlag(uint16_t flag) { ev("MSetFlag", {Cell(this), flag}); }

void CMapCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b, bool flag) {
    ev("CalcN", {Cell(this), static_cast<int32_t>(mode), a, b, flag ? 1 : 0});
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetN", {Cell(this)});
    return static_cast<uint8_t>(ret("GetN"));
}

CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    ev("GNC", {Cell(this), direction});
    int64_t k = ret("GNC");
    if (k == 0) return nullptr;
    return reinterpret_cast<CMapCell*>(CellBase()) + (k - 1);
}

uint8_t ADD_DIRECTION(uint8_t a, uint8_t b) {
    ev("AddDir", {a, b});
    int32_t n = 0;
    int32_t first = a;
    while (static_cast<uint8_t>(first) > 1) {
        first = first >> 1;
        ++n;
    }
    const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(b) << n);
    if (v > 0x80) return static_cast<uint8_t>((v >> 8) & 0xff);
    return static_cast<uint8_t>(v & 0xff);
}

uint8_t OPPOSITE(uint8_t d) {
    ev("Opp", {d});
    if (d == 0) return 0;
    if (d < 0x10) return static_cast<uint8_t>((d << 4) & 0xf0);
    return static_cast<uint8_t>((d >> 4) & 0xff);
}

void CCellBase::ModifyDirection(uint8_t direction) { ev("ModDir", {Cell(this), direction}); }

uint8_t CCellBase::GetDirection() const {
    ev("GetDir", {Cell(this)});
    return static_cast<uint8_t>(ret("GetDir"));
}

int32_t CMapCell::IsRide() const {
    ev("IsRide", {Cell(this)});
    return static_cast<int32_t>(ret("IsRide"));
}

int32_t LLCC_SystemGetRideQueue() {
    ev("RideQueue", {});
    return static_cast<int32_t>(ret("RideQueue"));
}

void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModNb", {Cell(this), mask}); }

void CObject::RecalculateBackOfQueue() { ev("BackOfQueue", {lab(this)}); }

int32_t LLCC_SystemGetRideTrack() {
    ev("RideTrack", {});
    return static_cast<int32_t>(ret("RideTrack"));
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return gTestInfo;
}

int32_t LLCC_DrawKartBlock(int32_t x, int32_t y, int32_t unused, int32_t flagA, int32_t flagB, uint8_t direction) {
    ev("Kart", {x, y, unused, flagA, flagB, direction});
    return static_cast<int32_t>(ret("Kart"));
}

uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetEnd", {*x, *y, rotation});
    *x = static_cast<int32_t>(ret("OEX"));
    *y = static_cast<int32_t>(ret("OEY"));
    return 0;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_pr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"first_x", &gLLCCFirstBlockX},
        {"first_y", &gLLCCFirstBlockY},
        {"e0cc", &gLLCCE0CC},
        {"c214", &gLLCCC214},
        {"db6c", &gLLCCDB6C},
        {"curobj", &gLLCMCurrentObject},
        {"e0dc", &gLLCCE0DC},
        {"e0e0", &gLLCCE0E0},
        {"47c4", &gLLCC47C4},
        {"bank3", &gSoundBanks[3]},
        {"world", &gGameWorld},
        {"recording", &gActionRecorder.mRecording},
        {"ridedb", &gRideDatabase},
        {"place", &gLLCRPlacement},
        {"d0", &gLLCRPlaceRotation},
        {"d4", &gLLCRPlaceY},
        {"d8", &gLLCRPlaceX},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_pr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_pr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(LLCRPlacement));
    out[n++] = static_cast<int64_t>(offsetof(LLCRPlacement, mEntranceDirection));
    return n;
}

int32_t tpw_pr_rideinfo_size() { return static_cast<int32_t>(sizeof(RideInfo)); }

// The answers of LLCR_AddRideToDB and GetRideInfo.
void tpw_pr_set_answers(uint64_t ride, uint64_t info) {
    gTestRide = reinterpret_cast<void*>(static_cast<uintptr_t>(ride));
    gTestInfo = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info));
}

void tpw_pr_set_grid(const int32_t* kinds, const int32_t* dirs) {
    std::memcpy(gTestKinds, kinds, sizeof(gTestKinds));
    std::memcpy(gTestDirs, dirs, sizeof(gTestDirs));
}

void tpw_pr_set_slot(uint64_t info, int32_t slot, int32_t value) {
    reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info))->Value(static_cast<RideBalanceSlot>(slot)).mInteger = value;
}

uint64_t tpw_pr_place(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t b, int32_t c) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LLCR_PlaceRideBlock(x, y, a, rotation, b, c)));
}

}  // extern "C"
