// Test library for CMapCell::IsHoarding, IsTrack, RefreshHFieldTile, ValidTypeChange and CalculateDirections (unit v5270;
// libtpw_unit_CMapCell_IsHoarding.dylib, tests/test_CMapCell_IsHoarding.py), main app:
//   src/game/park/MapCell.cpp   the five functions and the globals gIsTrackNoParent, gValidTypeChangeLock
//
// Fakes (the same events as the module's PowerPC stubs): CCellBase::GetTrackCellEquiv, ModifyDirection, CTrackCell::IsTrackX, IsTrack,
// IsHoarding, LLCCM_CreateQMesh, SingleHFieldTextureChange. gGameWorld (defined in another file) is defined here; it is only a base address
// the code computes track table pointers from, nothing is read through it.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;

namespace {

uint64_t gEquivPtr = 0;
uint64_t gCellPtr = 0;

// The label a pointer is logged as: 1 the equivalent cell, 2 the cell under test, else 100 + the track table index it lies at.
int64_t Lab(const void* p) {
    const uint64_t a = reinterpret_cast<uintptr_t>(p);
    if (a != 0 && a == gEquivPtr) return 1;
    if (a != 0 && a == gCellPtr) return 2;
    const int64_t table = static_cast<int64_t>(reinterpret_cast<uintptr_t>(gGameWorld)) + 0x1102d8;
    return (static_cast<int64_t>(a) - table) / 0x28 + 100;
}

}  // namespace

// ---- the fakes
const CTrackCell* CCellBase::GetTrackCellEquiv() const {
    ev("GetTrackCellEquiv", {Lab(this)});
    return reinterpret_cast<const CTrackCell*>(static_cast<uintptr_t>(gEquivPtr));
}

void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {Lab(this), static_cast<int64_t>(direction)}); }

int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {Lab(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Lab(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}

int32_t CTrackCell::IsHoarding() const {
    ev("IsHoarding", {Lab(this)});
    return static_cast<int32_t>(ret("IsHoarding"));
}

uint32_t LLCCM_CreateQMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    ev("CreateQMesh", {static_cast<int64_t>(x), static_cast<int64_t>(y), static_cast<int64_t>(index.mKind), static_cast<int64_t>(index.mIndex),
                       static_cast<int64_t>(index.mRotation), static_cast<int64_t>(restore)});
    return static_cast<uint32_t>(ret("CreateQMesh"));
}

void SingleHFieldTextureChange(int32_t x, int32_t y, const CCellBase::CELL_IndexType* tile) {
    ev("SingleHFieldTextureChange", {static_cast<int64_t>(x), static_cast<int64_t>(y), static_cast<int64_t>(tile->mKind),
                                     static_cast<int64_t>(tile->mIndex), static_cast<int64_t>(tile->mRotation)});
}

extern "C" {

uint64_t tpw_v5270_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"equiv", &gEquivPtr},
        {"cell", &gCellPtr},
        {"noparent", &gIsTrackNoParent},
        {"lock", &gValidTypeChangeLock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5270_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5270_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mTileData));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

uint32_t tpw_v5270_IsHoarding(uint64_t cell) { return static_cast<uint32_t>(reinterpret_cast<const CMapCell*>(cell)->IsHoarding()); }
uint32_t tpw_v5270_IsTrack(uint64_t cell) { return static_cast<uint32_t>(reinterpret_cast<const CMapCell*>(cell)->IsTrack()); }
void tpw_v5270_RefreshHFieldTile(uint64_t cell) { reinterpret_cast<CMapCell*>(cell)->RefreshHFieldTile(); }
uint32_t tpw_v5270_ValidTypeChange(uint64_t cell, int32_t mode) {
    return static_cast<uint32_t>(reinterpret_cast<CMapCell*>(cell)->ValidTypeChange(static_cast<CELL_MODES>(mode)));
}
void tpw_v5270_CalculateDirections(uint64_t cell, int32_t a, int32_t b) { reinterpret_cast<CMapCell*>(cell)->CalculateDirections(a, b); }

}  // extern "C"
