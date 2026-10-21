// Test library for CMapCell::Modify (unit v5637; libtpw_unit_CMapCell_Modify.dylib, tests/test_CMapCell_Modify.py), main app:
//   src/game/park/MapCell.cpp   CMapCell::Modify, and the globals gModifyLastMode, gModifyTypeRefused, gModifyPylonIndex,
//                                          gModifyNoParent and gModifyNoThing
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue; the cells the
// cell getters answer come from the test's tables (an index into them, 0 for NULL where the original can get NULL). The globals of other
// files the unit reads, defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ObjectParts.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "ThingReceivers.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
int32_t gLLCCC210;
int32_t gLLCCC214;
uint16_t gLLCCDB68;
int32_t gLLCCDE0B4;
int32_t gLLCCDE0B8;
int32_t gLLCC47C8;
int32_t gLLCC4800;
int32_t gLLCC4804;
int32_t gValidTypeChangeLock;
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

float BitsFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// The cells the getters answer (set by the test per case).
uint64_t gCells[8];
int32_t gCellCount;
uint64_t gTrackCells[8];
int32_t gTrackCellCount;

CMapCell* CellAt(int64_t index) {
    if (index <= 0 || index > gCellCount) return nullptr;
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(gCells[index - 1]));
}

// A checksum of a string (its length and a weighted byte sum), the same as the module's for the PowerPC's copy.
int64_t TextSum(const char* text) {
    uint32_t sum = 0;
    uint32_t n = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) {
        ++n;
        sum = sum * 31u + *p;
    }
    return static_cast<int64_t>((static_cast<uint64_t>(n) << 32) | sum);
}

// What Delete and LLCR_DeleteRide do to the cell system's two words in the test (the original restores them after).
void Clobber() {
    gLLCC4804 = 0x4804;
    gLLCC4800 = 0x4800;
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
int32_t LLCC_SystemGetMode() {
    ev("LLCC_SystemGetMode", {});
    return static_cast<int32_t>(ret("LLCC_SystemGetMode"));
}

void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("LLCC_SystemSetModeNR", {mode, flag ? 1 : 0}); }

void LLCM_SetIllegalPlacement(int32_t placement) { ev("LLCM_SetIllegalPlacement", {placement}); }

void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag) {
    ev("LLCR_DeleteRide", {x, y, flag});
    Clobber();
}

uint8_t LLCR_FixBridges(CObject* ride, bool a, bool b, bool c) {
    ev("LLCR_FixBridges", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("LLCR_FixBridges"));
}

uint8_t LLCR_FixBridgesReverse(CObject* ride, bool a, bool b, bool c) {
    ev("LLCR_FixBridgesReverse", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("LLCR_FixBridgesReverse"));
}

float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("GetWorldGSquareHeight", {x, y, extra == nullptr ? 0 : 1});
    return BitsFloat(static_cast<uint32_t>(ret("GetWorldGSquareHeight")));
}

void DC_AddSquareFaceToList(float x, float y, float z, uint8_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                            uint8_t unused2, int32_t keepPosition, DYN_Type type) {
    ev("DC_AddSquareFaceToList", {CanonBits(x), CanonBits(y), CanonBits(z), side, texture, rotation, CanonBits(unused1), large, unused2,
                                  keepPosition, static_cast<int32_t>(type)});
    ev("DC_Words", {gLLCCC210, gLLCCC214});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition, TextSum(format)}); }

void RemoveBridge(CTrackCell* cell, MapDelta a, MapDelta b) { ev("RemoveBridge", {lab(cell), a.mX, a.mY, b.mX, b.mY}); }

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {lab(this)});
    const int64_t index = ret("GetTrackCellEquiv");
    if (index <= 0 || index > gTrackCellCount) return nullptr;
    return reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(gTrackCells[index - 1]));
}

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {lab(this)});
    return CellAt(ret("GetMapCellEquiv"));
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {lab(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}

int32_t CMapCell::ConnectionType(CELL_MODES mode, int32_t flag) {
    ev("ConnectionType", {lab(this), static_cast<int32_t>(mode), flag});
    return static_cast<int32_t>(ret("ConnectionType"));
}

CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    ev("GetNeighbouringCell", {lab(this), delta->mX, delta->mY, flag});
    return CellAt(ret("GetNeighbouringCell"));
}

void CMapCell::CalculateDirections(int32_t a, int32_t b) { ev("CalculateDirections", {lab(this), a, b}); }

void CMapCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b, bool flag) {
    ev("CalculateNeighbours", {lab(this), static_cast<int32_t>(mode), a, b, flag ? 1 : 0});
}

void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {lab(this)}); }

void CMapCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) { ev("ModifyParentID", {lab(this), id.mIndex}); }

void CMapCell::SetPylonIndex(int16_t index) { ev("SetPylonIndex", {lab(this), static_cast<uint16_t>(index)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    const uint64_t offset = reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(gGameWorld);
    ev("GetObjectThingId", {static_cast<int64_t>(offset & 0xffffffffu)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetObjectThingId"));
    return id;
}

void CMapCell::Delete(int32_t a, int32_t b) {
    ev("Delete", {lab(this), a, b});
    Clobber();
}

int32_t CMapCell::IsPath() const {
    ev("IsPath", {lab(this)});
    return static_cast<int32_t>(ret("IsPath"));
}

int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {lab(this), flag});
    return static_cast<int32_t>(ret("IsFlag"));
}

int32_t CMapCell::IsRide() const {
    ev("IsRide", {lab(this)});
    return static_cast<int32_t>(ret("IsRide"));
}

int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {lab(this)});
    return static_cast<int32_t>(ret("IsEntrance"));
}

int32_t CMapCell::IsExit() const {
    ev("IsExit", {lab(this)});
    return static_cast<int32_t>(ret("IsExit"));
}

int32_t CMapCell::ModifyTypeBody(CELL_MODES mode) {
    ev("ModifyType", {lab(this), static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("ModifyType"));
}

int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {lab(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {lab(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}

int32_t CTrackCell::IsBridge() const {
    ev("IsBridge", {lab(this)});
    return static_cast<int32_t>(ret("IsBridge"));
}

int32_t CTrackCell::IsHoarding() const {
    ev("IsHoarding", {lab(this)});
    return static_cast<int32_t>(ret("IsHoarding"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {lab(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}

uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {lab(this)});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}

void CTrackCell::SetFlag(uint16_t flags) { ev("SetFlag", {lab(this), flags}); }

void CTrackCell::UnSetFlag(uint16_t flags) { ev("UnSetFlag", {lab(this), flags}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_mcm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLLCCC210", &gLLCCC210},
        {"gLLCCC214", &gLLCCC214},
        {"gLLCCDB68", &gLLCCDB68},
        {"gLLCCDE0B4", &gLLCCDE0B4},
        {"gLLCCDE0B8", &gLLCCDE0B8},
        {"gLLCC47C8", &gLLCC47C8},
        {"gLLCC4800", &gLLCC4800},
        {"gLLCC4804", &gLLCC4804},
        {"gValidTypeChangeLock", &gValidTypeChangeLock},
        {"gModifyLastMode", &gModifyLastMode},
        {"gModifyTypeRefused", &gModifyTypeRefused},
        {"gModifyPylonIndex", &gModifyPylonIndex},
        {"gModifyNoParent", &gModifyNoParent},
        {"gModifyNoThing", &gModifyNoThing},
        {"gGameWorld", &gGameWorld},
        {"gThingArray", &gThingArray[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_mcm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The cells the getters answer by index (1-based; 0 answers NULL).
void tpw_mcm_tables(const uint64_t* cells, int32_t n, const uint64_t* tracks, int32_t m) {
    gCellCount = n;
    for (int32_t i = 0; i < n; ++i) gCells[i] = cells[i];
    gTrackCellCount = m;
    for (int32_t i = 0; i < m; ++i) gTrackCells[i] = tracks[i];
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_mcm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(MapDelta));
    return n;
}

// ---- the function, with C types only (the cell as a pointer)
uint32_t tpw_mcm_modify(uint64_t cell, int32_t mode, int32_t a, int32_t b) {
    CMapCell* const self = reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell));
    return static_cast<uint32_t>(self->Modify(static_cast<CELL_MODES>(mode), a, b));
}

}  // extern "C"
