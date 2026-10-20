// Test library for CTrackCell::Modify (unit v5881; libtpw_unit_CTrackCell_Modify.dylib, tests/test_CTrackCell_Modify.py), main app:
//   src/TrackCell_flags_CTrackCell_Modify.cpp   CTrackCell::Modify, ModifyBody, ModifyLocked, the weak ModifyTypeBody, and the globals
//   gLLCCDC21C, gLLCC4854, gModifyNoParent, gModifyNoThing
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue (pointers the
// fakes answer are cell numbers: 0 NULL, k the k-th cell block the test hands over with tpw_tcm_cells). The globals of other files the
// unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
int32_t gLLCC4858;
int32_t gLLCC4800;
int32_t gLLCC4804;
uint8_t gLLCC4818;
CTrackCell* gLLCCDB48;
uint16_t gLLCCDB68;
int32_t gHoardingRight;
int32_t gHoardingBottom;
int32_t gHoardingLeft;
int32_t gHoardingTop;
int32_t gLLCCDE0B4;
int32_t gLLCCDE0B8;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};
const MapDelta gMapDeltaMinusY = {0, -1, 0};
const MapDelta gMapDeltaMinusX = {-1, 0, 0};

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

uintptr_t gCells[16];

void* Cell(int64_t number) {
    if (number <= 0 || number >= 16) return nullptr;
    return reinterpret_cast<void*>(gCells[number]);
}

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("GetWorldGSquareHeight", {x, y, lab(extra)});
    return FromBits(ret("GetWorldGSquareHeight"));
}

float GetTrackMapCellHeight(CMapCell* cell, float height, int32_t x, int32_t y) {
    ev("GetTrackMapCellHeight", {lab(cell), CanonBits(height), x, y});
    return FromBits(ret("GetTrackMapCellHeight"));
}

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {lab(this)});
    return static_cast<CMapCell*>(Cell(ret("GetMapCellEquiv")));
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {lab(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {lab(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

int32_t CTrackCell::ConnectionType(CELL_MODES mode, int32_t a, uint8_t b) {
    ev("ConnectionType", {lab(this), static_cast<int32_t>(mode), a, b});
    return static_cast<int32_t>(ret("ConnectionType"));
}

void DC_AddSquareFaceToList(float x, float y, float z, uint8_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                            uint8_t unused2, int32_t keepPosition, DYN_Type type) {
    ev("DC_AddSquareFaceToList", {CanonBits(x), CanonBits(y), CanonBits(z), side, texture, rotation, CanonBits(unused1), large, unused2,
                                  keepPosition, static_cast<int32_t>(type)});
}

CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("CTrackCell::GetNeighbouringCell", {lab(this), delta.mX, delta.mY, offset});
    return static_cast<CTrackCell*>(Cell(ret("CTrackCell::GetNeighbouringCell")));
}

CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    ev("CMapCell::GetNeighbouringCell", {lab(this), delta->mX, delta->mY, flag});
    return static_cast<CMapCell*>(Cell(ret("CMapCell::GetNeighbouringCell")));
}

void LLCM_SetIllegalPlacement(int32_t placement) { ev("LLCM_SetIllegalPlacement", {placement}); }

void CTrackCell::CalculateDirections(int32_t a, int32_t b) { ev("CalculateDirections", {lab(this), a, b}); }

void CTrackCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b) {
    ev("CalculateNeighbours", {lab(this), static_cast<int32_t>(mode), a, b});
}

uint8_t CTrackCell::IsStraight() const {
    ev("IsStraight", {lab(this)});
    return static_cast<uint8_t>(ret("IsStraight"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {lab(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}

uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {lab(this)});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}

void CMapCell::UnSetFlag(uint16_t flag) { ev("CMapCell::UnSetFlag", {lab(this), flag}); }
void CMapCell::SetHFieldTile() { ev("CMapCell::SetHFieldTile", {lab(this)}); }
void CMapCell::SetFlag(uint16_t flag) { ev("CMapCell::SetFlag", {lab(this), flag}); }
uint16_t CMapCell::GetFlag() {
    ev("CMapCell::GetFlag", {lab(this)});
    return static_cast<uint16_t>(ret("CMapCell::GetFlag"));
}

void CTrackCell::UnSetFlag(uint16_t flags) { ev("CTrackCell::UnSetFlag", {lab(this), flags}); }
void CTrackCell::SetFlag(uint16_t flags) { ev("CTrackCell::SetFlag", {lab(this), flags}); }
void CTrackCell::SetHFieldTile() { ev("CTrackCell::SetHFieldTile", {lab(this)}); }

void CTrackCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) {
    ev("ModifyParentID", {lab(this), id.mIndex});
}

void CTrackCell::ModifyType(CELL_MODES mode) {
    ev("ModifyType", {lab(this), static_cast<int32_t>(mode)});
    ret("ModifyType");
}

int32_t CTrackCell::ModifyTypeBody(CELL_MODES mode) {
    ev("ModifyType", {lab(this), static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("ModifyType"));
}

int32_t CMapCell::IsWater() const {
    ev("IsWater", {lab(this)});
    return static_cast<int32_t>(ret("IsWater"));
}

int32_t CMapCell::IsFreeform() const {
    ev("IsFreeform", {lab(this)});
    return static_cast<int32_t>(ret("IsFreeform"));
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {lab(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}

int32_t CTrackCell::IsHoarding() const {
    ev("IsHoarding", {lab(this)});
    return static_cast<int32_t>(ret("IsHoarding"));
}

// Records the map cell's index in the world's grid (the address is never dereferenced: gGameWorld is a fake address).
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    const intptr_t grid = reinterpret_cast<intptr_t>(gGameWorld->mMapCells);
    const intptr_t index = (reinterpret_cast<intptr_t>(this) - grid) / static_cast<intptr_t>(sizeof(CMapCell));
    ev("GetObjectThingId", {static_cast<int64_t>(index)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetObjectThingId"));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    const int64_t same = std::strcmp(format, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n") == 0 ? 1 : 0;
    ev("GAME_ASSERTMSG", {condition, same});
}

uint8_t LLCR_FixBridges(CObject* ride, bool a, bool b, bool c) {
    ev("LLCR_FixBridges", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("LLCR_FixBridges"));
}

uint8_t LLCR_FixBridgesReverse(CObject* ride, bool a, bool b, bool c) {
    ev("LLCR_FixBridgesReverse", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("LLCR_FixBridgesReverse"));
}

void CTrackCell::Delete(int32_t a, int32_t b, bool c) { ev("CTrackCell::Delete", {lab(this), a, b, c ? 1 : 0}); }

// Also overwrites the two drag start words, which shows that Modify puts its saved copies back.
int32_t LLCC_SystemGetMode() {
    ev("LLCC_SystemGetMode", {});
    gLLCC4800 = 0x11111111;
    gLLCC4804 = 0x22222222;
    return static_cast<int32_t>(ret("LLCC_SystemGetMode"));
}

void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag) { ev("LLCR_DeleteRide", {x, y, flag}); }

void CMapCell::Delete(int32_t a, int32_t b) { ev("CMapCell::Delete", {lab(this), a, b}); }

void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("LLCC_SystemSetModeNR", {mode, flag ? 1 : 0}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_tcm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLLCC4858", &gLLCC4858},
        {"gLLCC4854", &gLLCC4854},
        {"gLLCCDC21C", &gLLCCDC21C},
        {"gLLCC4800", &gLLCC4800},
        {"gLLCC4804", &gLLCC4804},
        {"gLLCC4818", &gLLCC4818},
        {"gLLCCDB48", &gLLCCDB48},
        {"gLLCCDB68", &gLLCCDB68},
        {"gHoardingRight", &gHoardingRight},
        {"gHoardingBottom", &gHoardingBottom},
        {"gHoardingLeft", &gHoardingLeft},
        {"gHoardingTop", &gHoardingTop},
        {"gModifyNoParent", &gTrackModifyNoParent},
        {"gModifyNoThing", &gTrackModifyNoThing},
        {"gLLCCDE0B4", &gLLCCDE0B4},
        {"gLLCCDE0B8", &gLLCCDE0B8},
        {"gGameWorld", &gGameWorld},
        {"gThingArray", &gThingArray[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_tcm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host address of gThingArray[index].mThing.
uint64_t tpw_tcm_thing(int32_t index) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gThingArray[index].mThing)); }

// The cell blocks the pointer fakes answer (number k is cells[k - 1]).
void tpw_tcm_cells(const uint64_t* cells, int32_t count) {
    for (int32_t i = 0; i < 16; ++i) gCells[i] = 0;
    for (int32_t i = 0; i < count && i < 15; ++i) gCells[i + 1] = static_cast<uintptr_t>(cells[i]);
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_tcm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    return n;
}

// ---- the function (the Mac's r3)
uint32_t tpw_tcm_modify(uint64_t cell, int32_t mode, int32_t a, int32_t b) {
    return static_cast<uint32_t>(reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(cell))->ModifyBody(static_cast<CELL_MODES>(mode), a, b));
}

}  // extern "C"
