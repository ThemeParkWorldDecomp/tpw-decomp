// LLCC_TraceTrack 0x1008ddfc (2,212 bytes): walks the track from one cell to another (two cells a step, along the direction byte), and with the
// flag set writes the directions, finds the first and last cell of the segment, deletes the cells of the cross-road stretch and breaks the
// track at the ends. Answers the number of cells walked, or -1 when a cell of the walk is no track.
#include <cstdint>

#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"

namespace {

// The track cell of a cell id (id 0 lands one cell before the grid), as the original computes it: world + 0x2d8 + 0x110000 + (id - 1) * 0x28.
CTrackCell* CellOfId(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + 0x110000 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// A track X cell stands for its parent.
CTrackCell* Resolve(CTrackCell* cell) {
    if (cell->IsTrackX()) return CellOfId(cell->mParentID);
    return cell;
}

int32_t CellX(uint16_t id) { return static_cast<int32_t>(static_cast<uint32_t>(id) - 1u) & 0x7f; }
int32_t CellY(uint16_t id) { return static_cast<int32_t>((static_cast<uint32_t>(id) - 1u) >> 7); }

void Step(uint8_t dir, int32_t& x, int32_t& y) {
    switch (dir) {
        case 1: y -= 2; break;
        case 4: x += 2; break;
        case 0x10: y += 2; break;
        case 0x40: x -= 2; break;
        default: break;
    }
}

}  // namespace

// The callers ignore the answer; other units' test fakes define LLCC_TraceTrack as void, so the header says void.
void LLCC_TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) {
    LLCC_TraceTrackCount(from, to, flag);
}

int32_t LLCC_TraceTrackCount(CTrackCell* from, CTrackCell* to, int32_t flag) {
    CTrackCell* startCell = nullptr;
    CTrackCell* endCell = nullptr;
    int32_t count = 0;
    int32_t startCross = 0;   // 0x74
    int32_t endCross = 0;     // 0x70

    from = Resolve(from);
    to = Resolve(to);
    int16_t segFrom = from->GetSegmentNumber();
    int16_t segTo = to->GetSegmentNumber();

    if (flag == 0 || (static_cast<int32_t>(segTo) - static_cast<int32_t>(segFrom)) >= 0) {
        uint16_t id = from->mCellId;
        int32_t lineEndX = -1;   // 0x68
        int32_t lineEndY = -1;   // 0x64
        int32_t x = CellX(id);
        int32_t y = CellY(id);
        int32_t lineX = x;
        int32_t lineY = y;
        uint8_t dir = from->GetDirection();
        int32_t prevX = -1;
        int32_t prevY = -1;
        bool done = false;
        do {
            CTrackCell* cell = CellOfId(static_cast<uint16_t>(x + (y << 7) + 1));
            cell = Resolve(cell);
            if (!cell->IsTrack()) return -1;
            int16_t seg = cell->GetSegmentNumber();
            if (flag != 0) {
                if (startCell == nullptr && seg == segFrom) {
                    startCell = cell;
                    startCross = 0;
                }
                if (startCell == nullptr && cell->IsCrossRoad()) {
                    startCross = 1;
                    startCell = cell;
                }
                if (seg == segTo) {
                    endCell = cell;
                    endCross = 0;
                }
                if (endCell == nullptr && cell->IsCrossRoad()) {
                    endCross = 1;
                    endCell = cell;
                }
            }
            if (cell->IsTrack() || cell->mType == static_cast<CELL_MODES>(0x19)) {
                if (cell->IsCorner()) {
                    if (count != 0) {
                        uint8_t other = OPPOSITE(dir);
                        if ((other & cell->GetNeighbours()) == 0) other = dir;
                        dir = static_cast<uint8_t>(other ^ cell->GetNeighbours());
                    }
                    lineEndX = prevX;
                    lineEndY = prevY;
                }
            }
            if (cell == to) {
                lineEndX = x;
                done = true;
                lineEndY = y;
            }
            if (flag != 0) {
                if (cell->IsCrossRoad()) {
                    cell->ModifyDirection(static_cast<uint8_t>(dir | cell->GetDirection()));
                } else {
                    cell->ModifyDirection(dir);
                }
            }
            count++;
            if (lineX != -1 && lineEndX != -1) {
                LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x10f), lineX, lineY, &lineEndX, &lineEndY);
                lineEndX = -1;
                lineEndY = -1;
                lineX = x;
                lineY = y;
            }
            prevX = x;
            prevY = y;
            Step(dir, x, y);
        } while (!done);

        if (flag != 0 && startCell != nullptr && endCell != nullptr && startCell != endCell) {
            id = startCell->mCellId;
            x = CellX(id);
            y = CellY(id);
            if (startCell->IsCrossRoad()) {
                dir = from->GetDirection();
            } else {
                dir = startCell->GetDirection();
            }
            bool stop = false;
            do {
                CTrackCell* cell = CellOfId(static_cast<uint16_t>(x + (y << 7) + 1));
                cell = Resolve(cell);
                cell->GetSegmentNumber();
                if (cell->IsTrack() || cell->mType == static_cast<CELL_MODES>(0x19)) {
                    if (cell->NumberOfDirections() == 1) {
                        dir = cell->GetDirection();
                    } else {
                        cell->ModifyDirection(static_cast<uint8_t>(cell->GetDirection() & ~dir));
                    }
                } else {
                    stop = true;
                }
                if (cell == endCell) stop = true;
                if (cell->IsCrossRoad()) {
                    cell->Delete(-1, -1, true);
                } else if (cell->IsStraight()) {
                    cell->Delete(0, 0, true);
                } else if (cell->IsCorner()) {
                    cell->Delete(0, 0, true);
                } else if (cell->NumberOfNeighbours() == 1) {
                    cell->Delete(0, 0, true);
                }
                Step(dir, x, y);
            } while (!stop);
        }
    }

    if (flag != 0) {
        bool breakable = from->IsStraight() || from->IsCorner();
        if (gLLCCDB6C != 0) {
            // The current object: the Mac's 32-bit word, the host keeps the whole pointer beside it while the two agree.
            char* object = (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C))
                               ? reinterpret_cast<char*>(gLLCMCurrentObject)
                               : reinterpret_cast<char*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
            BMP_FreeTrack(*reinterpret_cast<int32_t*>(object + 0x24));
        }
        if (static_cast<int32_t>(segTo) - static_cast<int32_t>(segFrom) <= 1) {
            uint16_t id = from->mCellId;
            if (breakable) LLCC_BreakTrack(CellX(id), CellY(id), 0);
        }
        if (startCross != 0) {
            uint16_t id = from->mCellId;
            if (breakable) LLCC_BreakTrack(CellX(id), CellY(id), 0);
        }
        if (endCross != 0) {
            uint8_t dir = from->GetDirection();
            uint16_t id = to->mCellId;
            if (breakable) LLCC_BreakTrack(CellX(id), CellY(id), OPPOSITE(dir));
        }
        if (startCell == endCell && startCell != nullptr && breakable) startCell->Delete(0, 0, true);
    }
    return count;
}
