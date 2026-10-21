// Test library for the cell system's rectangle clearer and hoarding walk (unit v5082; libtpw_unit_LLCC_Kill_Them_KILL_THEM_ALL.dylib,
// tests/test_LLCC_Kill_Them_KILL_THEM_ALL.py), main app:
//   src/game/park/LLCellRide.cpp   LLCC_Kill_Them_KILL_THEM_ALL, LLCC_BuildHoardingDB and the four extent words
//
// The callees outside the unit are recording fakes (the cell predicates and mutators, IsXYValid, LLCR_DeleteRide, OPPOSITE, ADD_DIRECTION,
// GetNeighbouringCell, LLCC_CreateCurrentHoardingImage); cells are named in the log by their id halfword.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files, defined here
uint8_t gLLCC4818;
CWorld* gGameWorld;
const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};

namespace {

int64_t Id(const CCellBase* cell) { return cell->mCellId; }

}  // namespace

// ---- the fakes
int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {Id(this), flag});
    return static_cast<int32_t>(ret("IsFlag"));
}
int32_t CMapCell::IsPath() const {
    ev("IsPath", {Id(this)});
    return static_cast<int32_t>(ret("IsPath"));
}
void CMapCell::Delete(int32_t a, int32_t b) { ev("Delete", {Id(this), a, b}); }
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}
void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag) { ev("DeleteRide", {x, y, flag}); }

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {Id(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Id(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {Id(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}
void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {Id(this), direction}); }
void CCellBase::RemoveNeighbour(uint8_t mask) { ev("RemoveNeighbour", {Id(this), mask}); }
uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {Id(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}
uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {Id(this)});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}
void CTrackCell::Modify(CELL_MODES mode, int32_t a, int32_t b) { ev("Modify", {Id(this), static_cast<int32_t>(mode), a, b}); }
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {Id(this)}); }
void CTrackCell::ModifyType(CELL_MODES mode) { ev("ModifyType", {Id(this), static_cast<int32_t>(mode)}); }
void CTrackCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) { ev("ModifyParentID", {Id(this), id.mIndex}); }
CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("GetNeighbouringCell", {Id(this), delta.mX, delta.mY, offset});
    int64_t id = ret("GetNeighbouringCell");
    if (id == 0) return nullptr;
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + 0x110000 + (id - 1) * 0x28);
}
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>((direction << 4) | (direction >> 4));
}
uint8_t ADD_DIRECTION(uint8_t first, uint8_t second) {
    ev("ADD_DIRECTION", {first, second});
    // (the fake: first rotated left by the index of the lowest set bit of second; the test's PowerPC fake does the same)
    unsigned s = second ? static_cast<unsigned>(__builtin_ctz(second)) & 7u : 0u;
    return static_cast<uint8_t>(((first << s) | (first >> (8 - s))) & 0xff);
}
void LLCC_CreateCurrentHoardingImage() { ev("CreateCurrentHoardingImage", {}); }

extern "C" {

uint64_t tpw_kt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"c4818", &gLLCC4818},
        {"minx", &gLLCCHoardMinX},
        {"miny", &gLLCCHoardMinY},
        {"maxx", &gLLCCHoardMaxX},
        {"maxy", &gLLCCHoardMaxY},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

void tpw_kt_setworld(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }

uint64_t tpw_kt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_kt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    return n;
}

uint32_t tpw_kt_kill(int32_t a, int32_t b, int32_t c, int32_t d) { return static_cast<uint32_t>(LLCC_Kill_Them_KILL_THEM_ALL(a, b, c, d)); }
uint32_t tpw_kt_build(int32_t a, int32_t b, int32_t c, int32_t d, uint32_t mode, uint32_t flag) {
    return static_cast<uint32_t>(LLCC_BuildHoardingDB(a, b, c, d, static_cast<char>(mode), flag != 0));
}

}  // extern "C"
