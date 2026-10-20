// The world's cells (main app of SimTheme Park 2.04b3; the inventory's file names for these are unreliable; unit w113): the three
// cell constructors and the two grid registrations. The six grid globals are defined by the unit's test file and, in the launcher,
// by the generated zero definitions (nothing else reads them).
#include <cstring>

#include "CellBase.h"
#include "MapCell.h"
#include "RegionEffectCell.h"
#include "TrackCell.h"

// 0x10102948 (92 bytes). The word at +0x3c is cleared first (the memset makes that redundant), then all 0x44 bytes are zeroed, then the
// type byte and the halfword 0x423f are set. The halfword is big-endian on the Mac (bytes 0x42 0x3f); here it is the value.
CMapCell::CMapCell() {
    mF3C = 0;
    std::memset(static_cast<void*>(this), 0, sizeof(CMapCell));
    mKind = 1;
    mCellId = 0x423f;
}

// 0x1012fb08 (68 bytes): all 0x28 bytes zeroed, then the type byte.
CTrackCell::CTrackCell() {
    std::memset(static_cast<void*>(this), 0, sizeof(CTrackCell));
    mKind = 2;
}

// 0x10120c10 (60 bytes): the five shorts are zeroed.
CRegionEffectCell::CRegionEffectCell() {
    std::memset(static_cast<void*>(this), 0, 10);
}

// 0x100f4780 (36 bytes). The end is base + (size << 14), the shift done in 32 bits (slwi), so a size of 0x40000 or more wraps there;
// the host keeps the 32-bit shift and adds in pointer width. The stores come in the order base, end, size.
void CCellBase::RegisterMapGrid(const CMapCell* grid, int32_t cellSize) {
    const uint8_t* const end = reinterpret_cast<const uint8_t*>(grid) + (static_cast<uint32_t>(cellSize) << 14);
    gMapGridBase = grid;
    gMapGridEnd = reinterpret_cast<const CMapCell*>(end);
    gMapGridCellSize = cellSize;
}

// 0x100f47e0 (36 bytes): the same for the track grid.
void CCellBase::RegisterTrackGrid(const CTrackCell* grid, int32_t cellSize) {
    const uint8_t* const end = reinterpret_cast<const uint8_t*>(grid) + (static_cast<uint32_t>(cellSize) << 14);
    gTrackGridBase = grid;
    gTrackGridEnd = reinterpret_cast<const CTrackCell*>(end);
    gTrackGridCellSize = cellSize;
}
