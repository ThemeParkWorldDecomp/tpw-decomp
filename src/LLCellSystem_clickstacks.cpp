// The click stacks of the queue and track tools (main app, LLCellSystem.cpp range 0x10092280 to 0x100936a8, SimTheme Park 2.04b3; unit w204):
// LLCC_SystemBacktrack 0x10092280, LLCC_SystemConstructQueueClickStack 0x10092644, LLCC_SystemConstructTrackClickStack 0x10092e44 and
// LLCC_SystemConstructReverseTrackClickStack 0x10093280. The stores and calls keep the listings' order: the logs compare them.
//
// Habits of the original that are kept:
//  - Every push on the stack is inlined by the compiler with LLCC_SystemAddToClickStack's rule (a count ABOVE 0x400 refuses, the entry 0x400 is
//    written one past the array: x lands in the count word and y in the word after it, whose high byte is gLLCC47C4; the count becomes x + 1),
//    except that the count is read again before every store and no answer is made. Written once here as PushClick.
//  - A cell is found by plain arithmetic on its 1 based id, (x + (y << 7) + 1) cut to 16 bits, with no test of x and y: the walks read cells
//    outside the grid when the cells they follow lead there (the original never tests the position before it reads a cell).
//  - The queue walk and the two track walks stop only on their tests (a direction that is none of the four compass bits never moves the position).
//  - The track walks step two cells at a time (the track cells lie on every second cell); the forward walk's stop at a start block of its own
//    parent MOVES two more cells, the reverse walk's does not.
//  - The forward track walk counts its passes in a register nobody reads, the reverse one too; both counters are left out. Backtrack tests its
//    pair of clicks for NULL twice after popping (the second test is dead).
#include <cstdint>

#include "CellBase.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"

// The click Backtrack pops (the Mac keeps it eight bytes before the copy of the stack; the name is invented).
LLCCClick gLLCCBacktrackClick = {0, 0};   // 0x102e07b0

namespace {

// The map cell and the track cell of a 1 based id: plain arithmetic on the world, as the Mac does it (id 0 lands one cell before the first).
CMapCell* MapCellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

CTrackCell* TrackCellOfId(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// The id of the cell at x and y, in 32 bits cut to 16 (the walks' positions are plain words).
uint16_t IdOfCell(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

// One push on the click stack (see the head of the file).
void PushClick(int32_t x, int32_t y) {
    const int32_t count = gLLCCClickStackCount;
    if (count > 0x400) return;
    if (count == 0x400) {
        gLLCCClickStackCount = x;
        gLLCC47C4 = static_cast<uint8_t>(static_cast<uint32_t>(y) >> 24);
    } else {
        gLLCCClickStack[count].mX = x;
        gLLCCClickStack[count].mY = y;
    }
    gLLCCClickStackCount = gLLCCClickStackCount + 1;
}

// The ride the cell system holds (gLLCCDB6C is a 32 bit pointer word on the Mac).
CObject* HeldRide() {
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}

// Backtrack's two ways out after a mode that sets the first block: with the flag and the bridge fixes. The held ride is read again before each call.
void FixBridgesOfHeldRide(bool fixBridges) {
    if (!fixBridges) return;
    if (gLLCCDB6C == 0) return;
    if (LLCR_FixBridges(HeldRide(), false, false, false) != 0) return;
    LLCR_FixBridgesReverse(HeldRide(), true, false, false);
}

// One step of the queue walk along a compass bit, one cell (0x10 down the y axis, 4 along x ...).
void StepOne(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 1; break;
        case 4: *x += 1; break;
        case 1: *y -= 1; break;
        case 0x40: *x -= 1; break;
        default: break;
    }
}

// The same step backwards.
void StepBack(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y -= 1; break;
        case 4: *x -= 1; break;
        case 1: *y += 1; break;
        case 0x40: *x += 1; break;
        default: break;
    }
}

// The track walks' step, two cells.
void StepTwo(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 2; break;
        case 4: *x += 2; break;
        case 1: *y -= 2; break;
        case 0x40: *x -= 2; break;
        default: break;
    }
}

// What both track walks do with a cell that holds track: a corner turns the direction (the opposite of the one it came with, xor the cell's
// neighbours) and is pushed, a crossroad or any other cell only sets the overlap counter. IsStraight is asked and its answer thrown away.
void TrackCellShape(CTrackCell* cell, uint8_t* direction, int32_t x, int32_t y) {
    if (cell->IsCorner() != 0) {
        const uint8_t opposite = OPPOSITE(*direction);
        const uint8_t neighbours = cell->GetNeighbours();
        *direction = static_cast<uint8_t>(opposite ^ neighbours);
        PushClick(x, y);
        cell->SetOverlapCounter(1);
    } else if (cell->IsCrossRoad() != 0) {
        cell->SetOverlapCounter(1);
    } else {
        cell->IsStraight();
        cell->SetOverlapCounter(0);
    }
}

}  // namespace

// 0x10092280, 912 bytes. The mode is read once. Mode 0x15 deletes the last brace; the modes 0xe, 0x13, 4, 0x3b and 0x16 do nothing. Otherwise the
// last click is popped from the stack: the click at the bottom (index 0) is NOT taken off (the count is put back) and counts as "no click".
// Answers 1 when a click was popped, else 0.
int32_t LLCC_SystemBacktrack(CObject* obj, bool fixBridges) {
    bool firstBlockFlag = false;
    bool queued = false;
    const int32_t mode = gLLCC480C;
    if (mode == 0x15) {
        DeleteLastTrackBrace();
        return 0;
    }
    if (mode == 0xe || mode == 0x13 || mode == 4 || mode == 0x3b || mode == 0x16) return 0;

    LLCCClick* click = nullptr;
    const int32_t count = gLLCCClickStackCount;
    if (count > 0) {
        gLLCCClickStackCount = count - 1;
        const int32_t index = gLLCCClickStackCount;
        gLLCCBacktrackClick.mX = gLLCCClickStack[index].mX;
        gLLCCBacktrackClick.mY = gLLCCClickStack[index].mY;
        if (index != 0) {
            click = &gLLCCBacktrackClick;
        } else {
            gLLCCClickStackCount = gLLCCClickStackCount + 1;
        }
    }

    if (click == nullptr) {
        if (mode == 0xb || mode == 0x10 || mode == 0x35) {
            LLCC_FixFirstBlock(firstBlockFlag);
            FixBridgesOfHeldRide(fixBridges);
        }
        LLCC_SystemSetModeNR(mode, true);
        return 0;
    }

    LLCM_SetIllegalPlacement(0);
    if (mode == 1 || mode == 3 || mode == 0x34) {
        LLCC_SystemSetMode(0x34, true);
        queued = true;
    } else if (mode == 0xb || mode == 0x10 || mode == 0x19 || mode == 0x35) {
        LLCC_SystemSetMode(0x35, true);
        gLLCCE288 = 0x80;
        gLLCCE28C = 0x80;
    }
    LLCM_LeftMouseButtonUp(click->mX, click->mY, 0, 0);

    // The stack may have grown in the call; its last click now becomes the pair (none when it is empty).
    const int32_t remaining = gLLCCClickStackCount;
    if (remaining > 0) {
        gLLCCBacktrackClick.mX = gLLCCClickStack[remaining - 1].mX;
        gLLCCBacktrackClick.mY = gLLCCClickStack[remaining - 1].mY;
    } else {
        click = nullptr;
    }
    if (remaining == 1) firstBlockFlag = true;
    if (click != nullptr) {
        // Mode 3 skips the second mouse up when the click's map cell is a path (mType 1).
        const CMapCell* cell = nullptr;
        if (mode == 3) cell = MapCellOfId(IdOfCell(click->mX, click->mY));
        if (mode != 3 || cell->mType != 1) {
            LLCM_LeftMouseButtonUp(click->mX, click->mY, 0, 0);
        }
        gLLCC4804 = click->mX;
        gLLCC4800 = click->mY;
    }
    LLCC_SystemSetModeNR(mode, true);
    if (queued) {
        if (obj != nullptr) {
            obj->RecalculateBackOfQueue();
        } else if (gLLCCDB6C != 0) {
            HeldRide()->RecalculateBackOfQueue();
        }
    }
    if (mode == 0xb || mode == 0x10 || mode == 0x35) {
        LLCC_FixFirstBlock(firstBlockFlag);
        FixBridgesOfHeldRide(fixBridges);
    }
    return 1;
}

// 0x10092644, 1980 bytes. Starts on the ride's entry cell (the halfword at +0x32) and follows the queue from there, pushing the corners; answers 0, or
// the direction byte when the walk met a cell that is not a queue. The walk's state: x and y (words), the direction, the answer and the stop flag.
int32_t LLCC_SystemConstructQueueClickStack(CObject* obj) {
    bool stop = false;
    int32_t answer = 0;
    bool moved = false;

    const uint16_t startId = obj->mF32;
    int32_t x = static_cast<int32_t>(startId) - 1;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(x) >> 7);
    x = x & 0x7f;
    gLLCCClickStackCount = 0;
    CMapCell* const start = MapCellOfId(IdOfCell(x, y));
    uint8_t direction = start->GetNeighbours();

    // A start cell with one neighbour looks at it first: a cell that is not a queue, or a queue of the same parent, is pushed; a queue of another
    // parent is where the walk begins (and ends: it goes straight to the last push).
    const MapDelta* source = nullptr;
    int32_t nx = x;
    int32_t ny = y;
    switch (direction) {
        case 0x10: source = &gMapDeltaPlusY; ny = y + 1; break;
        case 4: source = &gMapDeltaPlusX; nx = x + 1; break;
        case 1: source = &gMapDeltaMinusY; ny = y - 1; break;
        case 0x40: source = &gMapDeltaMinusX; nx = x - 1; break;
        default: break;
    }
    if (source != nullptr) {
        MapDelta delta = *source;
        CMapCell* const next = start->GetNeighbouringCell(&delta, 1);
        if (next != nullptr) {
            if (next->IsQueue() == 0) {
                PushClick(nx, ny);
            } else if (next->mParentID == start->mParentID) {
                PushClick(nx, ny);
            } else {
                moved = true;
                x = nx;
                y = ny;
            }
        }
    }

    if (!moved) {
        if (direction == 0 || !CMap::IsXYValid(x, y)) {
            // No neighbour (or the cell is outside the level): one step from the entry cell along its direction turned by the ride's rotation.
            const int32_t rotation = obj->mRotation;
            const uint8_t cellDirection = MapCellOfId(startId)->GetDirection();
            uint8_t turn = 0;
            switch (rotation) {
                case 0: turn = 1; break;
                case 0x5a: turn = 4; break;
                case 0xb4: turn = 0x10; break;
                case 0x10e: turn = 0x40; break;
                default: break;
            }
            StepOne(ADD_DIRECTION(cellDirection, turn), &x, &y);
        } else {
            do {
                CMapCell* const cell = MapCellOfId(IdOfCell(x, y));
                if (cell->IsQueue() == 0) {
                    stop = true;
                    answer = direction;
                } else if (cell->IsCorner() != 0) {
                    const uint8_t opposite = OPPOSITE(direction);
                    const uint8_t neighbours = cell->GetNeighbours();
                    direction = static_cast<uint8_t>(opposite ^ neighbours);
                    PushClick(x, y);
                    cell->SetOverlapCounter(1);
                } else {
                    cell->SetOverlapCounter(0);
                }
                if (cell->IsEntrance() == 0) {
                    if (cell->NumberOfNeighbours() == 1) stop = true;
                    if (cell->IsPath() != 0) {
                        // A path where the queue ends: the link between the cell and the one behind it is cut (unless that is an entrance, which
                        // keeps the walk on the path cell).
                        const int32_t savedX = x;
                        const int32_t savedY = y;
                        StepBack(direction, &x, &y);
                        CMapCell* const behind = MapCellOfId(IdOfCell(x, y));
                        if (behind->IsEntrance() != 0) {
                            x = savedX;
                            y = savedY;
                        } else {
                            behind->RemoveNeighbour(direction);
                            behind->SetHFieldTile();
                        }
                        cell->RemoveNeighbour(OPPOSITE(direction));
                        cell->SetHFieldTile();
                        stop = true;
                    }
                }
                if (!stop) StepOne(direction, &x, &y);
            } while (!stop);
        }
    }
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
    return answer;
}

// 0x10092e44, 1016 bytes. The track walked from the ride's start towards its end, two cells at a time (the track cells lie on every second cell).
void LLCC_SystemConstructTrackClickStack(CObject* obj) {
    bool stop = false;
    const uint16_t objectId = IdOfCell(obj->mBase.mCellX, obj->mBase.mCellY);
    int32_t x = (static_cast<int32_t>(objectId) - 1) & 0x7f;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(objectId) - 1) >> 7);
    uint8_t direction = LLCR_OffsetStartOfTrack(&x, &y, obj->mRotation);
    gLLCCE0CC = direction;
    gLLCCClickStackCount = 0;
    if (TrackCellOfId(IdOfCell(x, y))->IsJustTrack() != 0) PushClick(x, y);
    do {
        CTrackCell* const cell = TrackCellOfId(IdOfCell(x, y));
        if (cell->IsJustTrack() != 0) {
            TrackCellShape(cell, &direction, x, y);
            if (cell->IsCorner() == 0 && (cell->GetNeighbours() & direction) == 0) stop = true;
            CTrackCell* const next = cell->GetNeighbouringCell(direction, 2);
            if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
                const uint8_t back = OPPOSITE(direction);
                if ((back & next->GetNeighbours()) != 0) {
                    stop = true;
                    StepTwo(direction, &x, &y);
                }
            }
        }
        if (!stop) StepTwo(direction, &x, &y);
    } while (!stop);
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
}

// 0x10093280, 1064 bytes. The same walk from the other end of the track (LLCR_OffsetEndOfTrack) back towards the start. The first block's cell
// (gLLCCFirstBlockX and Y, when they are inside the level) gets its overlap counter cleared when the walk passes it; the walk also stops when its
// own first cell is not track at all. A start block of the cell's own parent stops the walk where it stands.
void LLCC_SystemConstructReverseTrackClickStack(CObject* obj) {
    bool stop = false;
    uint16_t firstBlockId = 0;
    const uint16_t objectId = IdOfCell(obj->mBase.mCellX, obj->mBase.mCellY);
    int32_t x = (static_cast<int32_t>(objectId) - 1) & 0x7f;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(objectId) - 1) >> 7);
    uint8_t direction = LLCR_OffsetEndOfTrack(&x, &y, obj->mRotation);
    gLLCCE0CC = direction;
    if (CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY) != 0) {
        const int32_t firstY = gLLCCFirstBlockY;
        const int32_t firstX = gLLCCFirstBlockX;
        firstBlockId = IdOfCell(firstX, firstY);
    }
    gLLCCClickStackCount = 0;
    CTrackCell* const startCell = TrackCellOfId(IdOfCell(x, y));
    if (startCell->IsJustTrack() != 0) PushClick(x, y);
    if (startCell->IsTrack() == 0) stop = true;
    do {
        CTrackCell* const cell = TrackCellOfId(IdOfCell(x, y));
        if (cell->IsJustTrack() != 0) {
            TrackCellShape(cell, &direction, x, y);
            if (cell->mCellId == firstBlockId) cell->SetOverlapCounter(0);
            if (cell->IsCorner() == 0 && (cell->GetNeighbours() & direction) == 0) stop = true;
            CTrackCell* const next = cell->GetNeighbouringCell(direction, 2);
            if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
                const uint8_t back = OPPOSITE(direction);
                if ((back & next->GetNeighbours()) != 0) stop = true;
            }
        }
        if (!stop) StepTwo(direction, &x, &y);
    } while (!stop);
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
}
