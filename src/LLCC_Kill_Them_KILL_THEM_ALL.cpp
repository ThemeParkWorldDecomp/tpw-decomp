// LLCellSystem (LLCellRide.cpp of the main app, unit v5082): LLCC_Kill_Them_KILL_THEM_ALL 0x1008ebc0 (1,212 bytes) and LLCC_BuildHoardingDB
// 0x1008e6e0 (1,196 bytes). The cells are reached through the world: map cells 0x44 bytes at gGameWorld + 0x2d8, track cells 0x28 bytes at
// gGameWorld + 0x2d8 + 0x110000, ids 1 + x + (y << 7) as a halfword.
#include <cstdint>

#include "CellBase.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

int32_t gLLCCHoardMinX;   // 0x102de0c8
int32_t gLLCCHoardMinY;   // 0x102de0c4
int32_t gLLCCHoardMaxX;   // 0x102de0c0
int32_t gLLCCHoardMaxY;   // 0x102de0bc

namespace {

CMapCell* MapCellAt(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

// One scan along a line: the cell at (x, y) is the origin; `step` moves along the axis. Returns nothing: clears ok like the original.
// (Written out four times in the original; the loops differ only in the axis, the step and the bound.)
}  // namespace

// 0x1008ebc0 (1,212 bytes): for every cell of the rectangle that is a hoarding-like cell (mode 4, 9 or 10) whose run of equal parent cells
// along +x, -x, +y and -y stays inside the rectangle (plus one) and has no cell flagged 0x10, the ride is deleted; then every path cell of
// the rectangle is deleted. Always 1.
int32_t LLCC_Kill_Them_KILL_THEM_ALL(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int32_t xMin = x1 < x2 ? x1 : x2;
    int32_t yMin = y1 < y2 ? y1 : y2;
    int32_t xMax = x2 < x1 ? x1 : x2;
    int32_t yMax = y2 < y1 ? y1 : y2;
    // (the original picks the max with `p3 < p1 ? p3 : p1` style compares on the stack copies: min(a,b) when a<b else b, max the mirror)
    xMin = (x1 < x2) ? x1 : x2;
    xMax = (x2 < x1) ? x1 : x2;
    yMin = (y1 < y2) ? y1 : y2;
    yMax = (y2 < y1) ? y1 : y2;

    for (int32_t y = yMin; y <= yMax; y++) {
        for (int32_t x = xMin; x <= xMax; x++) {
            CMapCell* origin = MapCellAt(x, y);
            int32_t mode = origin->mType;
            bool candidate;
            if (mode < 9)
                candidate = (mode == 4);
            else
                candidate = (mode < 0xb);
            if (!candidate) continue;
            bool ok = true;
            int32_t cx = x;
            int32_t cy = y;
            CMapCell* cell;
            // +x
            do {
                cell = MapCellAt(cx, y);
                if (cell->IsFlag(0x10)) ok = false;
                if (cx > xMax + 1) ok = false;
                int32_t m = cell->mType;
                cx++;
                if (!(m == 4 || m == 9 || m == 10)) break;
                if (cell->mParentID != origin->mParentID) break;
            } while (CMap::IsXYValid(cx, y));
            cx = x;
            if (ok) {
                // -x
                do {
                    cell = MapCellAt(cx, y);
                    if (cell->IsFlag(0x10)) ok = false;
                    if (cx < xMin - 1) ok = false;
                    int32_t m = cell->mType;
                    cx--;
                    if (!(m == 4 || m == 9 || m == 10)) break;
                    if (cell->mParentID != origin->mParentID) break;
                } while (CMap::IsXYValid(cx, y));
            }
            cy = y;
            if (ok) {
                // +y
                do {
                    cell = MapCellAt(x, cy);
                    if (cell->IsFlag(0x10)) ok = false;
                    if (cy > yMax + 1) ok = false;
                    int32_t m = cell->mType;
                    cy++;
                    if (!(m == 4 || m == 9 || m == 10)) break;
                    if (cell->mParentID != origin->mParentID) break;
                } while (CMap::IsXYValid(x, cy));
            }
            cy = y;
            if (ok) {
                // -y
                do {
                    cell = MapCellAt(x, cy);
                    if (cell->IsFlag(0x10)) ok = false;
                    if (cy < yMin - 1) ok = false;
                    int32_t m = cell->mType;
                    cy--;
                    if (!(m == 4 || m == 9 || m == 10)) break;
                    if (cell->mParentID != origin->mParentID) break;
                } while (CMap::IsXYValid(x, cy));
            }
            if (ok) LLCR_DeleteRide(x, y, 0);
        }
    }
    for (int32_t y = yMin; y <= yMax; y++) {
        for (int32_t x = xMin; x <= xMax; x++) {
            CMapCell* cell = MapCellAt(x, y);
            if (cell->IsPath()) cell->Delete(0, 0);
        }
    }
    return 1;
}

// 0x1008e6e0 (1,196 bytes): walks the hoarding from (x, y) cell by cell (two cells a step) along the direction byte, fixing corners and
// crossroads and giving the neighbouring cells to the hoarding, until the walk has passed (right, bottom); records the extents and redraws the
// current hoarding image. Always 1.
int32_t LLCC_BuildHoardingDB(int32_t right, int32_t bottom, int32_t left, int32_t top, char mode, bool flag) {
    int32_t x = right;
    int32_t y = bottom;
    int32_t targetX = left;
    int32_t targetY = top;
    uint8_t dir = static_cast<uint8_t>(mode);
    int32_t done = 0;
    gLLCCHoardMinX = 0x400;
    gLLCCHoardMinY = 0x400;
    gLLCCHoardMaxX = 0;
    gLLCCHoardMaxY = 0;
    do {
        uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
        CTrackCell* cell = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + 0x110000 +
                                                          static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
        if (cell->mType == 0x19 || (gLLCC4818 != 0 && cell->NumberOfNeighbours() != 0)) {
            if (cell->IsCorner()) {
                uint8_t other = OPPOSITE(dir);
                if ((other & cell->GetNeighbours()) == 0) other = dir;
                dir = static_cast<uint8_t>(other ^ cell->GetNeighbours());
            }
            if (cell->IsCrossRoad()) {
                uint8_t found = 0;
                uint8_t probe = ADD_DIRECTION(dir, 0x40);
                do {
                    if (probe & cell->GetNeighbours()) found = probe;
                    probe = ADD_DIRECTION(probe, 4);
                } while (found == 0);
                if (dir == found) {
                    uint8_t opposite = OPPOSITE(dir);
                    cell->RemoveNeighbour(static_cast<uint8_t>(~(dir | opposite)));
                    cell->Modify(static_cast<CELL_MODES>(0x81), 0, 0);
                    cell->SetHFieldTile();
                } else {
                    cell->ModifyDirection(found);
                    cell->RemoveNeighbour(dir);
                    cell->RemoveNeighbour(OPPOSITE(found));
                    cell->Modify(static_cast<CELL_MODES>(0x81), 0, 0);
                    cell->SetHFieldTile();
                }
                dir = found;
            }
        } else {
            dir = 0;
        }
        if (x == targetX && y == targetY) done = 1;
        if (flag && dir != cell->GetDirection()) {
            cell->ModifyDirection(dir);
            cell->Modify(static_cast<CELL_MODES>(0x81), 0, 0);
        }
        const MapDelta* deltas[3] = {&gMapDeltaPlusY, &gMapDeltaPlusX, &gMapDeltaPlusXPlusY};
        for (const MapDelta* source : deltas) {
            MapDelta delta;
            delta.mX = source->mX;
            delta.mY = source->mY;
            delta.mFlag = source->mFlag;
            CTrackCell* neighbour = cell->GetNeighbouringCell(delta, 1);
            if (neighbour != nullptr) {
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> parent;
                parent.mIndex = cell->mCellId;
                neighbour->ModifyParentID(parent);
                neighbour->ModifyType(static_cast<CELL_MODES>(0xc));
            }
        }
        switch (dir) {
            case 1: y -= 2; break;
            case 4: x += 2; break;
            case 0x10: y += 2; break;
            case 0x40: x -= 2; break;
            default: break;
        }
        if (x < gLLCCHoardMinX) gLLCCHoardMinX = x;
        if (y < gLLCCHoardMinY) gLLCCHoardMinY = y;
        if (x > gLLCCHoardMaxX) gLLCCHoardMaxX = x;
        if (y > gLLCCHoardMaxY) gLLCCHoardMaxY = y;
    } while (done == 0);
    LLCC_CreateCurrentHoardingImage();
    return 1;
}
