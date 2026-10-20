// Unit v5059 (main app, LLCellMouse.cpp on the Mac): LLCM_CoverUpgrade (0x100828e4, 888 bytes) and LLCM_RebuildUpgradeBridges
// (0x10082cf0, 1116 bytes). Both look at the four neighbours of the cell (x, y) of the map: a neighbour that is a track cell flagged
// 0x10 with the same id halfword (+0x10 of its map cell) pushes the coordinates by two cells.
#include <cstdint>

#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"

namespace {

uint8_t* WorldBytes() { return reinterpret_cast<uint8_t*>(gGameWorld); }

uint16_t IdOf(CCellBase* mapCell) { return *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(mapCell) + 0x10); }

// The cell of (x, y) as the two functions find it: the track cell (this) and the map cell that holds the id.
void FindCells(int32_t x, int32_t y, CTrackCell*& track, CCellBase*& map) {
    const uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
    map = reinterpret_cast<CCellBase*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44);
    track = map->GetTrackCellEquiv();
    if (track->IsTrackX()) {
        const uint16_t id = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(track) + 0x10);
        track = reinterpret_cast<CTrackCell*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000);
        map = reinterpret_cast<CCellBase*>(track->GetMapCellEquiv());
    }
}

// Whether the neighbour of `track` at `delta` is a flagged (0x10) track cell with the same id.
bool NeighbourMatches(CTrackCell* track, const MapDelta& delta, uint16_t id) {
    MapDelta copy = delta;
    CTrackCell* other = track->GetNeighbouringCell(copy, 2);
    if (other == nullptr) return false;
    CCellBase* otherMap = reinterpret_cast<CCellBase*>(other->GetMapCellEquiv());
    if (!other->IsFlag(0x10)) return false;
    if (IdOf(otherMap) != id) return false;
    return other->IsTrack() != 0;
}

}  // namespace

// 0x100828e4 (888 bytes)
void LLCM_CoverUpgrade(int32_t* x, int32_t* y) {
    CTrackCell* track;
    CCellBase* map;
    FindCells(*x, *y, track, map);
    if (!track->IsFlag(0x10)) return;
    const uint16_t id = IdOf(map);
    if (NeighbourMatches(track, gMapDeltaMinusY, id)) *y = *y - 2;
    if (NeighbourMatches(track, gMapDeltaPlusX, id)) *x = *x + 2;
    if (NeighbourMatches(track, gMapDeltaPlusY, id)) *y = *y + 2;
    if (NeighbourMatches(track, gMapDeltaMinusX, id)) *x = *x - 2;
}

// 0x10082cf0 (1116 bytes)
void LLCM_RebuildUpgradeBridges(int32_t x, int32_t y) {
    CTrackCell* track;
    CCellBase* map;
    int32_t dx = 0;
    int32_t dy = 0;
    const uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
    CCellBase* first = reinterpret_cast<CCellBase*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44);
    track = first->GetTrackCellEquiv();
    map = first;
    if (track->IsTrackX()) {
        const uint16_t id = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(track) + 0x10);
        track = reinterpret_cast<CTrackCell*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000);
        map = reinterpret_cast<CCellBase*>(track->GetMapCellEquiv());
        const int32_t cellIndex = *reinterpret_cast<uint16_t*>(map);
        x = (cellIndex - 1) & 0x7f;
        y = static_cast<int32_t>(static_cast<uint32_t>(cellIndex - 1) >> 7);
    }
    if (track->IsFlag(0x10)) {
        const uint16_t id = IdOf(map);
        if (NeighbourMatches(track, gMapDeltaMinusY, id)) dy = -2;
        if (NeighbourMatches(track, gMapDeltaPlusX, id)) dx = 2;
        if (NeighbourMatches(track, gMapDeltaPlusY, id)) dy = 2;
        if (NeighbourMatches(track, gMapDeltaMinusX, id)) dx = -2;
    }
    int32_t endX = x + dx;
    int32_t endY = y + dy;
    int32_t fromX = x;
    int32_t fromY = y;
    if (x > endX) {
        endX -= 2;
        fromX += 2;
    } else if (x < endX) {
        endX += 2;
        fromX -= 2;
    }
    if (y > endY) {
        endY -= 2;
        fromY += 2;
    } else if (y < endY) {
        endY += 2;
        fromY -= 2;
    }
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x85), fromX, fromY, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x86), fromX, fromY, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x81), fromX, fromY, &endX, &endY);
}
