// The park's inside and the hoarding edit check in LLCellSystem.cpp of the main app (SimTheme Park 2.04b3; unit w206): LLCC_FillInsideOfPark
// (0x10093b0c, 2,836 bytes) and LLCC_IsHoardingEditLegal (0x1009464c, 3,488 bytes). The calls keep the listing's order: the logs compare them.
//
// The map flag 0x40 marks the land outside the park (inferred: the cost loop of the edit check charges every such cell of the new area, and the
// fill below clears it from the cells inside the hoarding). Track cells flagged 0x20 are the locked hoarding (LLCC_LockHoarding).
//
// The names of the eight words are invented (the header says so). Cell arithmetic is the Mac's plain 16 bit id arithmetic on the world block, so the
// scans of the fill (west, south and east) have no bound of their own: they end at a locked cell, and a hoarding that is not locked walks them out of
// the world.
#include <cstdint>

#include "ActionRecorder.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"

int32_t gLLCCEdgeCount[4];         // 0x102e05a0
uint8_t gLLCCEdgeMarks[4][0x80];   // 0x102e05b0

namespace {

// The balance row after WaterTrackCell (Costs.MapCell, Mac +0x5cc in src/MainBalance.cpp): the price of one cell of land.
constexpr int kMapCellCostSlot = 369;

// The cell id of (x, y): 1 + x + (y << 7), 16 bits (the shift is the 32-bit one of the Mac).
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + ((static_cast<uint32_t>(y) << 7) + 1u));
}

// The track cell of a 1 based id: plain arithmetic on the world, as the Mac does it (id 0 lands one cell before the first).
CTrackCell* TrackCellOfId(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// The map cell of a 1 based id (the same arithmetic).
CMapCell* MapCellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

// A map cell and its three neighbours towards +y, +x +y and +x (asked in that order, each with a fresh copy of the compass offset). A
// neighbour outside the level is NULL and the Mac reads through it.
struct Square {
    CMapCell* mCell[4];
};

Square SquareAt(CMapCell* map) {
    Square square;
    square.mCell[0] = map;
    MapDelta d1 = gMapDeltaPlusY;
    square.mCell[1] = map->GetNeighbouringCell(&d1, 1);
    MapDelta d2 = gMapDeltaPlusXPlusY;
    square.mCell[2] = map->GetNeighbouringCell(&d2, 1);
    MapDelta d3 = gMapDeltaPlusX;
    square.mCell[3] = map->GetNeighbouringCell(&d3, 1);
    return square;
}

// Whether one of the four cells has the flag 0x40 (the Mac stops asking at the first that has).
bool AnyFlagged(const Square& square) {
    if (square.mCell[0]->IsFlag(0x40) != 0) return true;
    if (square.mCell[1]->IsFlag(0x40) != 0) return true;
    if (square.mCell[2]->IsFlag(0x40) != 0) return true;
    return square.mCell[3]->IsFlag(0x40) != 0;
}

// Takes the flag 0x40 off the four cells and re-tiles each one, cell by cell.
void ClearAndRetile(const Square& square) {
    square.mCell[0]->UnSetFlag(0x40);
    square.mCell[0]->SetHFieldTile();
    square.mCell[1]->UnSetFlag(0x40);
    square.mCell[1]->SetHFieldTile();
    square.mCell[2]->UnSetFlag(0x40);
    square.mCell[2]->SetHFieldTile();
    square.mCell[3]->UnSetFlag(0x40);
    square.mCell[3]->SetHFieldTile();
}

// A cell the inward scan has reached: when it is not locked, its map cell and the three after it lose the flag 0x40 (if any of them has it).
void ClearInside(CTrackCell* scanned) {
    if (scanned->IsFlag(0x20) != 0) return;
    CMapCell* map = scanned->GetMapCellEquiv();
    const Square square = SquareAt(map);
    if (AnyFlagged(square)) ClearAndRetile(square);
}

// The four scans across the park, one for each heading of the walk (the heading is the direction of the walk along the hoarding): the cells at
// two cell steps from the walk's cell up to and including the first locked one. Heading 0x10 scans towards -x, 4 towards +y, 1 towards +x.
void ScanTowardsMinusX(int32_t x, int32_t y) {
    int32_t k = x;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(k - 2, y));
        ClearInside(scanned);
        locked = scanned->IsFlag(0x20);
        k -= 2;
    } while (locked == 0);
}

void ScanTowardsPlusY(int32_t x, int32_t y) {
    int32_t k = y;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(x, k + 2));
        ClearInside(scanned);
        locked = scanned->IsFlag(0x20);
        k += 2;
    } while (locked == 0);
}

void ScanTowardsPlusX(int32_t x, int32_t y) {
    int32_t k = x;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(k + 2, y));
        ClearInside(scanned);
        locked = scanned->IsFlag(0x20);
        k += 2;
    } while (locked == 0);
}

// Heading 0x40 scans towards -y from the cell two rows up; it also stops (after the cell that made the row negative) at the top of the level, and
// `reachedTop` stays set for the rest of the fill, so a later scan of this kind scans one cell only.
void ScanTowardsMinusY(int32_t x, int32_t y, bool* reachedTop) {
    int32_t k = y - 2;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(x, k));
        ClearInside(scanned);
        k -= 2;
        if (k < 0) *reachedTop = true;
        locked = scanned->IsFlag(0x20);
    } while (locked == 0 && !*reachedTop);
}

// The walk's move of two cells for a heading.
void Move(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 2; break;
        case 4: *x += 2; break;
        case 1: *y -= 2; break;
        case 0x40: *x -= 2; break;
        default: break;
    }
}

// The sides of the edited rectangle, in the order of the tally words (gLLCCEdgeCount, gLLCCEdgeMarks): the column at the larger x, the row at the
// larger y, the column at the smaller x and the row at the smaller y.
enum Side { kHixColumn = 0, kHiyRow = 1, kLoxColumn = 2, kLoyRow = 3 };

// What the check counts along one side: the run of freeform dry cells met in a row, the hoarding cells, and of those the straight ones with the
// neighbour byte of the side's direction and all the others.
struct SideTally {
    int32_t mRun;
    int32_t mHoarding;
    int32_t mStraight;
    int32_t mOther;
};

// One cell on a side of the rectangle. Two freeform dry cells in a row make the edit illegal (`bad`); the mark says whether the cell lies inside the
// park (no flag 0x40) or is hoarding; `atEnd` is a cell at either end of the side (a corner when it is hoarding); `straightBytes` is 0x11 on a row
// and 0x44 on a column.
void TallySideCell(CTrackCell* cell, Side side, SideTally* tally, bool atEnd, uint8_t straightBytes, int32_t* corners, bool* bad) {
    CMapCell* map = cell->GetMapCellEquiv();
    if (map->IsFreeform() == 0 || map->IsWater() != 0) {
        tally->mRun = 0;
    } else {
        tally->mRun = tally->mRun + 1;
        if (tally->mRun > 1) {
            tally->mRun = 0;
            *bad = true;
        }
    }
    if (cell->IsHoarding() != 0) tally->mHoarding = tally->mHoarding + 1;
    uint8_t mark = 0;
    if (map->IsFlag(0x40) == 0 || cell->IsHoarding() != 0) mark = 1;
    const int32_t slot = gLLCCEdgeCount[side];
    gLLCCEdgeCount[side] = slot + 1;
    gLLCCEdgeMarks[side][slot] = mark;
    if (cell->IsHoarding() != 0) {
        if (atEnd) *corners = *corners + 1;
        if (cell->IsStraight() == 0 || cell->GetNeighbours() != straightBytes) {
            tally->mOther = tally->mOther + 1;
        } else {
            tally->mStraight = tally->mStraight + 1;
        }
    }
}

}  // namespace

// 0x10093b0c, 2,836 bytes. Walks the locked hoarding as LLCC_LockHoarding does (from (right, bottom), heading 4, two cells a step; the stop and
// the corner turn are the same) and at every hoarding cell on the walk scans across the park to the next locked cell: every unlocked cell the scan
// passes gives up the flag 0x40 (with its +y, +x +y and +x neighbours) and has its tile set again. Then every hoarding cell left in the level that is
// not locked (a hoarding cell behind a track cell is found through the parent) is deleted, its overlap counter cleared and its map cell and the three
// after it freed the same way.
void LLCC_FillInsideOfPark() {
    uint8_t direction = 4;
    bool stop = false;
    int32_t steps = 0;
    bool reachedTop = false;
    int32_t x = gHoardingRight;
    int32_t y = gHoardingBottom;
    gLLCCE0CC = 4;
    do {
        CTrackCell* cell = TrackCellOfId(CellId(x, y));
        if (cell->mType == 0x19) {
            switch (direction) {
                case 0x10: ScanTowardsMinusX(x, y); break;
                case 4: ScanTowardsPlusY(x, y); break;
                case 1: ScanTowardsPlusX(x, y); break;
                case 0x40: ScanTowardsMinusY(x, y, &reachedTop); break;
                default: break;
            }
            if (cell->IsCorner() != 0) {
                direction = OPPOSITE(direction);
                direction = static_cast<uint8_t>(direction ^ cell->GetNeighbours());
            }
        }
        CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
        if (next != nullptr && next->mType != 0x19) stop = true;
        if (cell->NumberOfNeighbours() == 1 && steps != 0) stop = true;
        if (!stop) Move(direction, &x, &y);
        ++steps;
    } while (!stop);
    for (int32_t row = 0; row < gLLCCHeight - 1; ++row) {
        for (int32_t column = 0; column < gLLCCWidth - 1; ++column) {
            CTrackCell* cell = TrackCellOfId(CellId(column, row));
            if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
            if (cell->IsHoarding() != 0 && cell->IsFlag(0x20) == 0) {
                CMapCell* map = cell->GetMapCellEquiv();
                const Square square = SquareAt(map);
                if (map->mType == 0x15) {
                    cell->QuickDelete();
                } else {
                    cell->Delete(0, 0, false);
                }
                cell->SetOverlapCounter(0);
                ClearAndRetile(square);
            }
        }
    }
}

// 0x1009464c, 3,488 bytes. Whether the rectangle from (x1, y1) to (x2, y2) (corners in any order) may become the park's hoarding: 1, or 0 for a
// corner outside the level, a rectangle two cells wide or less, an edge loop that visits (right, bottom) or (left, top) of the hoarding's extents on
// a side of the rectangle, two freeform dry cells in a row along a side, two hoarding corners and no straight hoarding piece on any side (after the
// fix below), an area that is all inside or all outside the park with fewer than two hoarding cells on each side, a refusal of LLCC_SoIsItLegal
// (its low byte) and, when `charge` is set, land that costs more than the bank holds (gLLCCC204 is set to 1 then). The tallies are left in
// gLLCCEdgeCount and gLLCCEdgeMarks (0x80 bytes a side, the unused marks 0xff).
int32_t LLCC_IsHoardingEditLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool charge) {
    const int32_t lox = x2 < x1 ? x2 : x1;
    const int32_t loy = y2 < y1 ? y2 : y1;
    const int32_t hix = x1 < x2 ? x2 : x1;
    const int32_t hiy = y1 < y2 ? y2 : y1;
    gLLCCEdgeCount[kLoyRow] = 0;
    gLLCCEdgeCount[kLoxColumn] = 0;
    gLLCCEdgeCount[kHiyRow] = 0;
    gLLCCEdgeCount[kHixColumn] = 0;
    bool bad = false;
    bool broke = false;
    for (int32_t i = 0; i < 0x80; ++i) {
        gLLCCEdgeMarks[kLoyRow][i] = 0xff;
        gLLCCEdgeMarks[kLoxColumn][i] = 0xff;
        gLLCCEdgeMarks[kHiyRow][i] = 0xff;
        gLLCCEdgeMarks[kHixColumn][i] = 0xff;
    }
    gLLCC47CC = 0;
    if (!CMap::IsXYValid(lox, loy)) return 0;
    if (!CMap::IsXYValid(hix, hiy)) return 0;
    if (hix - lox <= 2 || hiy - loy <= 2) return 0;
    SideTally sides[4] = {};
    int32_t corners = 0;
    // The cell of the first corner is asked for its flag; the answer is not used.
    MapCellOfId(CellId(x1, y1))->IsFlag(0x40);
    for (int32_t x = lox; x <= hix; x += 2) {
        if (x == gHoardingRight && (loy == gHoardingBottom || hiy == gHoardingBottom)) return 0;
        if (x == gHoardingLeft && (loy == gHoardingTop || hiy == gHoardingTop)) return 0;
        TallySideCell(TrackCellOfId(CellId(x, loy)), kLoyRow, &sides[kLoyRow], x == lox || x == hix, 0x11, &corners, &bad);
        TallySideCell(TrackCellOfId(CellId(x, hiy)), kHiyRow, &sides[kHiyRow], x == lox || x == hix, 0x11, &corners, &bad);
    }
    for (int32_t y = loy; y <= hiy; y += 2) {
        if ((lox == gHoardingRight || hix == gHoardingRight) && y == gHoardingBottom) return 0;
        if ((lox == gHoardingLeft || hix == gHoardingLeft) && y == gHoardingTop) return 0;
        TallySideCell(TrackCellOfId(CellId(lox, y)), kLoxColumn, &sides[kLoxColumn], y == loy || y == hiy, 0x44, &corners, &bad);
        TallySideCell(TrackCellOfId(CellId(hix, y)), kHixColumn, &sides[kHixColumn], y == loy || y == hiy, 0x44, &corners, &bad);
    }
    // A side with more than one hoarding cell that is not straight gets one straight piece added, or, when the cell at the corner it starts from
    // is hoarding (the first point for the sides at loy and lox, the second point for the sides at hiy and hix), the opposite side loses one.
    CTrackCell* const first = TrackCellOfId(CellId(x1, y1));
    CTrackCell* const second = TrackCellOfId(CellId(x2, y2));
    if (sides[kLoyRow].mOther > 1) {
        if (first->mType == 0x19) {
            sides[kHiyRow].mStraight = sides[kHiyRow].mStraight - 1;
        } else {
            sides[kLoyRow].mStraight = sides[kLoyRow].mStraight + 1;
        }
    }
    if (sides[kLoxColumn].mOther > 1) {
        if (first->mType == 0x19) {
            sides[kHixColumn].mStraight = sides[kHixColumn].mStraight - 1;
        } else {
            sides[kLoxColumn].mStraight = sides[kLoxColumn].mStraight + 1;
        }
    }
    if (sides[kHiyRow].mOther > 1) {
        if (second->mType == 0x19) {
            sides[kLoyRow].mStraight = sides[kLoyRow].mStraight - 1;
        } else {
            sides[kHiyRow].mStraight = sides[kHiyRow].mStraight + 1;
        }
    }
    if (sides[kHixColumn].mOther > 1) {
        if (second->mType == 0x19) {
            sides[kLoxColumn].mStraight = sides[kLoxColumn].mStraight - 1;
        } else {
            sides[kHixColumn].mStraight = sides[kHixColumn].mStraight + 1;
        }
    }
    if (corners == 2 && sides[kLoyRow].mStraight == 0 && sides[kHiyRow].mStraight == 0 && sides[kLoxColumn].mStraight == 0 &&
        sides[kHixColumn].mStraight == 0) {
        return 0;
    }
    if (bad) return 0;
    int32_t inside = 0;
    int32_t outside = 0;
    for (int32_t y = loy; y <= hiy; ++y) {
        for (int32_t x = lox; x <= hix; ++x) {
            CTrackCell* cell = TrackCellOfId(CellId(x, y));
            CMapCell* map = cell->GetMapCellEquiv();
            if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
            if (map->IsFlag(0x40) == 0 || cell->IsHoarding() != 0) {
                ++inside;
            } else {
                ++outside;
            }
        }
    }
    if ((inside == 0 || outside == 0) && sides[kLoyRow].mHoarding < 2 && sides[kLoxColumn].mHoarding < 2 && sides[kHiyRow].mHoarding < 2 &&
        sides[kHixColumn].mHoarding < 2) {
        return 0;
    }
    LLCC_CopyCurrentToNewImage();
    LLCC_AddNewAreaToImage(lox, loy, hix, hiy);
    if (static_cast<uint8_t>(LLCC_SoIsItLegal()) == 0) return 0;
    if (charge) {
        for (int32_t y = loy + 2; y < hiy; ++y) {
            for (int32_t x = lox + 2; x < hix; ++x) {
                CMapCell* map = MapCellOfId(CellId(x, y));
                if (map->IsFlag(0x40) != 0) {
                    gLLCC47CC = static_cast<int32_t>(static_cast<uint32_t>(gLLCC47CC) + static_cast<uint32_t>(gMainBalance.mValues[kMapCellCostSlot].mInteger));
                    if (gActionRecorder.mRecording == 0) {
                        const int32_t cost = gLLCC47CC;
                        CBankAccount* account = gGameWorld->GetBankAccount();
                        const int32_t balance = account->GetBalance();
                        if (static_cast<int32_t>(static_cast<uint32_t>(balance) - static_cast<uint32_t>(cost)) < 0) broke = true;
                    } else {
                        broke = false;
                    }
                }
            }
        }
        if (broke) {
            gLLCCC204 = 1;
            return 0;
        }
    }
    return 1;
}
