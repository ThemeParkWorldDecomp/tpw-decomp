// CMapCell::IsHoarding 0x1009ca9c, IsTrack 0x1009c96c, RefreshHFieldTile 0x1009cf64, ValidTypeChange 0x1009b054, CalculateDirections 0x1009af80
// (unit v5270; main app).
#include <cstdint>

#include "CellBase.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"

// 0x1029e224: the parent id IsTrack answers 0 for (initial value 0).
uint16_t gIsTrackNoParent;
// 0x102e4834: ValidTypeChange refuses mode 3 on a mode 1 cell while it is nonzero (initial value 0).
int32_t gValidTypeChangeLock;

namespace {

// The track cell a parent id names: the track table lies in the world at +0x1102d8, 0x28 bytes each, id 1 first (id 0 lands one cell before).
const CTrackCell* TrackCellOfParent(uint16_t id) {
    const uint8_t* world = reinterpret_cast<const uint8_t*>(gGameWorld);
    const int32_t offset = (static_cast<int32_t>(id) - 1) * 0x28;
    return reinterpret_cast<const CTrackCell*>(world + 0x2d8 + static_cast<int64_t>(offset) + 0x110000);
}

}  // namespace

// 0x1009ca9c, 128 bytes: the track cell the cell stands for; when that is a "track X" cell, the one its parent id names instead (no sentinel
// test here). Answers CTrackCell::IsHoarding of it.
int32_t CMapCell::IsHoarding() const {
    const CTrackCell* cell = GetTrackCellEquiv();
    if (cell->IsTrackX()) cell = TrackCellOfParent(cell->mParentID);
    return cell->IsHoarding();
}

// 0x1009c96c, 176 bytes: as IsHoarding, but a track X cell whose parent id is the sentinel answers 0 and CTrackCell::IsTrack is asked.
int32_t CMapCell::IsTrack() const {
    const CTrackCell* cell = GetTrackCellEquiv();
    if (cell->IsTrackX()) {
        if (cell->mParentID == gIsTrackNoParent) return 0;
        cell = TrackCellOfParent(cell->mParentID);
    }
    return cell->IsTrack();
}

// 0x1009cf64, 228 bytes: clears the mesh; the height-field tile is set at the cell's place (x = (id - 1) & 0x7f, y = (id - 1) >> 7, 32 bits
// so id 0 gives 0x1ffffff). A tile of kind 2 first gets a queue mesh and then the plain tile {0, 8, 0}.
void CMapCell::RefreshHFieldTile() {
    const uint32_t id = mCellId;
    mMeshInstance = 0;
    const int32_t x = static_cast<int32_t>((id - 1) & 0x7f);
    const int32_t y = static_cast<int32_t>((id - 1) >> 7);
    if (static_cast<int32_t>(mTileData[0]) == 2) {
        CELL_IndexType index = {mTileData[0], mTileData[1], mTileData[2]};
        mMeshInstance = LLCCM_CreateQMesh(x, y, index, 1);
        CELL_IndexType plain = {0, 8, 0};
        SingleHFieldTextureChange(x, y, &plain);
    } else {
        SingleHFieldTextureChange(x, y, reinterpret_cast<const CELL_IndexType*>(&mTileData[0]));
    }
}

// 0x1009b054, 200 bytes: whether the cell may change to the mode.
int32_t CMapCell::ValidTypeChange(CELL_MODES mode) {
    const int32_t m = static_cast<int32_t>(mode);
    const int32_t t = static_cast<int32_t>(mType);
    if (m == 0) return 1;
    if (m == 4 && t == 4) return 0;
    if (m == 0x15 && t == 0x15) return 1;
    if (m == 1 && t == 3) return 1;
    if (m == t || t == 0) return 1;
    if (m == 4 && t == 1) return 1;
    if (m == 3 && t == 1 && gValidTypeChangeLock == 0) return 1;
    return 0;
}

// 0x1009af80, 160 bytes: sets the direction bits for a step (-1/+1 in the first argument: 4 / 0x40; in the second: 0x10 / 1).
void CMapCell::CalculateDirections(int32_t a, int32_t b) {
    if (a == -1) ModifyDirection(4);
    if (a == 1) ModifyDirection(0x40);
    if (b == -1) ModifyDirection(0x10);
    if (b == 1) ModifyDirection(1);
}
