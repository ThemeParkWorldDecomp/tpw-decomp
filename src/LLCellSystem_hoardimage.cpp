// The park's hoarding in LLCellSystem.cpp of the main app (SimTheme Park 2.04b3; unit w201): the walk along it (the click stack, the lock flag), the
// default and balance file hoardings, the revision of its corners, and the image of the park (current and new, a 0x43 by 0x43 grid of half cells)
// with the flood fill and the legality checks that read it. The stores and calls keep the listing's order: the logs compare them.
//
// The names of the words are invented (the header says so). The two images are one array: the Mac keeps the new image (0x102de198) and the current
// one (0x102df321) back to back, so an area handed to LLCC_AddNewAreaToImage that reaches past the new image writes into the current one.
#include <cstddef>
#include <cstdint>

#include "CellBase.h"
#include "HoardingsBalance.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "ui_main.h"

int32_t gLLCCReviseAnswer = 1;                           // 0x1029df30
uint8_t gLLCCHoardingImages[2 * 0x43 * 0x43];            // 0x102de198 (new), 0x102df321 (current)

namespace {

constexpr int32_t kImageSide = 0x43;
constexpr int32_t kImageSize = kImageSide * kImageSide;   // 0x1189

uint8_t* NewImage() { return gLLCCHoardingImages; }
uint8_t* CurrentImage() { return gLLCCHoardingImages + kImageSize; }

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

// The walk's click stack push, inlined in LLCC_SystemConstructHoardingClickStack: only a count ABOVE 0x400 is refused (signed), so the entry 0x400
// is written one past the array: on the Mac x lands in the count word itself and y in the word after it, whose first (high) byte is gLLCC47C4;
// the count then becomes x + 1 (the same model as LLCC_SystemAddToClickStack's; the other three bytes of that word and a negative count are not modelled).
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

// The pieces of the walk along the hoarding that the two walkers share (the listings repeat them): the heading after a corner (the click stack's walker
// keeps the push between the heading and the stop tests, so it writes the corner out itself), the stop tests (`steps` is the number of steps made so far)
// and the move of two cells.
void TurnAtCorner(CTrackCell* cell, uint8_t* direction) {
    if (cell->mType == 0x19 && cell->IsCorner() != 0) {
        *direction = OPPOSITE(*direction);
        *direction = static_cast<uint8_t>(*direction ^ cell->GetNeighbours());
    }
}

void StopTests(CTrackCell* cell, uint8_t direction, int32_t steps, bool* stop) {
    CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
    if (next != nullptr && next->mType != 0x19) *stop = true;
    if (cell->NumberOfNeighbours() == 1 && steps != 0) *stop = true;
}

void Move(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 2; break;
        case 4: *x += 2; break;
        case 1: *y -= 2; break;
        case 0x40: *x -= 2; break;
        default: break;
    }
}

}  // namespace

// 0x100936f4, 512 bytes. Starts at (right, bottom) heading 4, pushes the start, then walks the hoarding two cells at a time: at a corner the heading
// becomes the opposite of the heading XOR the cell's neighbours (and the cell is pushed), the walk stops when the next cell is not hoarding, or when
// the cell has one neighbour and this is not the first step. The end is pushed too and left in gLLCC4804 (x) and gLLCC4800 (y). The walk has no step limit.
void LLCC_SystemConstructHoardingClickStack() {
    uint8_t direction = 4;
    bool stop = false;
    int32_t steps = 0;
    int32_t x = gHoardingRight;
    int32_t y = gHoardingBottom;
    gLLCCE0CC = 4;
    gLLCCClickStackCount = 0;
    gLLCCClickStack[0].mX = x;
    gLLCCClickStack[0].mY = y;
    gLLCCClickStackCount = gLLCCClickStackCount + 1;
    do {
        CTrackCell* cell = TrackCellOfId(CellId(x, y));
        if (cell->mType == 0x19 && cell->IsCorner() != 0) {
            direction = OPPOSITE(direction);
            direction = static_cast<uint8_t>(direction ^ cell->GetNeighbours());
            PushClick(x, y);
        }
        StopTests(cell, direction, steps, &stop);
        if (!stop) Move(direction, &x, &y);
        ++steps;
    } while (!stop);
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
}

// 0x10093934, 432 bytes. The same walk without the stack: every cell on the way gets the lock flag 0x20 set (lock) or cleared, BEFORE its corner test.
void LLCC_LockHoarding(bool lock) {
    uint8_t direction = 4;
    bool stop = false;
    int32_t steps = 0;
    int32_t x = gHoardingRight;
    int32_t y = gHoardingBottom;
    gLLCCE0CC = 4;
    do {
        CTrackCell* cell = TrackCellOfId(CellId(x, y));
        if (lock) {
            cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() | 0x20));
        } else {
            cell->UnSetFlag(0x20);
        }
        TurnAtCorner(cell, &direction);
        StopTests(cell, direction, steps, &stop);
        if (!stop) Move(direction, &x, &y);
        ++steps;
    } while (!stop);
}

// 0x100956a8, 204 bytes: the six clicks of the default hoarding, relative to (right, bottom) (read once).
void LLCC_DrawDefaultHoardings() {
    const int32_t x = gHoardingRight;
    const int32_t y = gHoardingBottom;
    LLCM_LeftMouseButtonUp(x, y, 0, 0);
    LLCM_LeftMouseButtonUp(x + 2, y, 0, 0);
    LLCM_LeftMouseButtonUp(x + 2, y + 4, 0, 0);
    LLCM_LeftMouseButtonUp(x - 10, y + 4, 0, 0);
    LLCM_LeftMouseButtonUp(x - 10, y, 0, 0);
    LLCM_LeftMouseButtonUp(x - 8, y, 0, 0);
}

// 0x100957a4, 156 bytes: the balance file's pairs of offsets (the table's values 1 and 2, then 3 and 4 ...) are clicked relative to (right, bottom) until
// an x offset of 0x200 or the 64th pair; the value is read BEFORE the count is tested (so at the 65th pair the Mac reads value 129). The two extents
// are read again for every click. The Mac leaves 1 in r3, which nobody reads.
void LLCC_DrawHoardingsFromBalanceFile(CHoardingsBalance* balance) {
    int32_t i = 0;
    for (;;) {
        if (balance->mValues[1 + 2 * i].mInteger == 0x200 || i > 0x3f) break;
        LLCM_LeftMouseButtonUp(gHoardingRight + balance->mValues[1 + 2 * i].mInteger, gHoardingBottom + balance->mValues[2 + 2 * i].mInteger, 0, 0);
        ++i;
    }
}

// 0x100961c0, 424 bytes: redraws the hoarding as the rectangle of the two corners (in any order): the cell system's mode is 0x19 and the hoarding
// flag byte 1 while the five clicks go round the rectangle; then the database is built from the extents (its answer kept), the hoarding is locked,
// the inside of the park filled and the hoarding unlocked, and the park's darkening recalculated.
void LLCC_ReviseHoarding(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    const int32_t lowX = x2 < x1 ? x2 : x1;
    const int32_t highX = x1 < x2 ? x2 : x1;
    const int32_t lowY = y2 < y1 ? y2 : y1;
    const int32_t highY = y1 < y2 ? y2 : y1;
    gLLCC4818 = 1;
    LLCC_SystemSetMode(0x19, false);
    LLCM_LeftMouseButtonUp(lowX, lowY, 0, 0);
    LLCM_LeftMouseButtonUp(highX, lowY, 0, 0);
    LLCM_LeftMouseButtonUp(highX, highY, 0, 0);
    LLCM_LeftMouseButtonUp(lowX, highY, 0, 0);
    LLCM_LeftMouseButtonUp(lowX, lowY, 0, 0);
    LLCC_SystemSetMode(0, false);
    UI_SetGameWindowCursor(1);
    gLLCCReviseAnswer = LLCC_BuildHoardingDB(gHoardingRight, gHoardingBottom, gHoardingLeft, gHoardingTop, 4, true);
    LLCC_LockHoarding(true);
    LLCC_FillInsideOfPark();
    LLCC_LockHoarding(false);
    RecalculateParkDarkening();
    gLLCC4818 = 0;
}

// 0x10096398, 712 bytes: fills the new image from (x, y) with `fill` (oldValue is never read): a cell is open when it is in the grid (0 to 0x41 each
// way) and holds ' ' or '@'. The cell itself, then four blocks (left, right, above, below), each marking its neighbour and recursing with the four calls
// the listing gives (several of them on cells already filled). A fill of ' ' or '@' would never end.
void LLCC_FloodFill(int32_t x, int32_t y, uint8_t oldValue, uint8_t fill) {
    uint8_t* image = NewImage();
    if (x > -1 && x < 0x42 && y > -1 && y < 0x42) {
        const int32_t row = x * kImageSide;
        uint8_t* cell = image + row + y;
        uint8_t value = *cell;
        if (value == ' ' || value == '@') {
            *cell = fill;
            if (x - 1 > -1) {
                if (x - 1 < 0x42 && y > -1 && y < 0x42) {
                    cell = image + row + y - kImageSide;
                    value = *cell;
                    if (value == ' ' || value == '@') {
                        *cell = fill;
                        LLCC_FloodFill(x - 2, y, oldValue, fill);
                        LLCC_FloodFill(x, y, oldValue, fill);
                        LLCC_FloodFill(x - 1, y - 1, oldValue, fill);
                        LLCC_FloodFill(x - 1, y + 1, oldValue, fill);
                    }
                }
            }
            if (x + 1 > -1 && x + 1 < 0x42 && y > -1 && y < 0x42) {
                cell = image + row + y + kImageSide;
                value = *cell;
                if (value == ' ' || value == '@') {
                    *cell = fill;
                    LLCC_FloodFill(x, y, oldValue, fill);
                    LLCC_FloodFill(x + 2, y, oldValue, fill);
                    LLCC_FloodFill(x + 1, y - 1, oldValue, fill);
                    LLCC_FloodFill(x + 1, y + 1, oldValue, fill);
                }
            }
            if (x > -1) {
                if (x < 0x42 && y - 1 > -1 && y - 1 < 0x42) {
                    cell = image + row + y - 1;
                    value = *cell;
                    if (value == ' ' || value == '@') {
                        *cell = fill;
                        LLCC_FloodFill(x - 1, y - 1, oldValue, fill);
                        LLCC_FloodFill(x + 1, y - 1, oldValue, fill);
                        LLCC_FloodFill(x, y - 2, oldValue, fill);
                        LLCC_FloodFill(x, y, oldValue, fill);
                    }
                }
            }
            if (x > -1) {
                if (x < 0x42 && y + 1 > -1 && y + 1 < 0x42) {
                    cell = image + row + y + 1;
                    value = *cell;
                    if (value == ' ' || value == '@') {
                        *cell = fill;
                        LLCC_FloodFill(x - 1, y + 1, oldValue, fill);
                        LLCC_FloodFill(x + 1, y + 1, oldValue, fill);
                        LLCC_FloodFill(x, y, oldValue, fill);
                        LLCC_FloodFill(x, y + 2, oldValue, fill);
                    }
                }
            }
        }
    }
}

// 0x1009668c, 372 bytes: the current image from the world, one byte per two by two cells (the last cell of the pair wins): ' ' first, then '@' for a
// hoarding cell, else '#' unless the cell's map cell is flagged 0x40 or the byte already holds '@'. A track X cell stands for its parent. The bounds
// are read again on every pass.
void LLCC_CreateCurrentHoardingImage() {
    uint8_t* image = CurrentImage();
    for (int32_t y = 0; y < gLLCCHeight; ++y) {
        for (int32_t x = 0; x < gLLCCWidth; ++x) {
            uint8_t* byte = image + (x >> 1) * kImageSide + (y >> 1);
            *byte = ' ';
            if (CMap::IsXYValid(x, y) != 0) {
                CTrackCell* cell = TrackCellOfId(CellId(x, y));
                if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
                CMapCell* mapCell = cell->GetMapCellEquiv();
                if (cell->IsHoarding() != 0) {
                    *byte = '@';
                } else if (mapCell->IsFlag(0x40) == 0 && *byte != '@') {
                    *byte = '#';
                }
            }
        }
    }
}

// 0x10096838, 108 bytes: the current image shifted one row and one column (+0x44) into the new one, 0x42 by 0x42 bytes, with the new image's border
// (first and last row and column) set to ' ' on the way (the outer loop is the column of the source, the inner one its row; the last byte written is 0x1188).
void LLCC_CopyCurrentToNewImage() {
    uint8_t* newImage = NewImage();
    uint8_t* current = CurrentImage();
    for (int32_t i = 0; i < 0x42; ++i) {
        for (int32_t j = 0; j < 0x42; ++j) {
            newImage[j * kImageSide + i + 0x44] = current[j * kImageSide + i];
        }
        newImage[i] = ' ';
        newImage[i + 0x1103] = ' ';
        newImage[i * kImageSide] = ' ';
        newImage[i * kImageSide + 0x41] = ' ';
    }
}

// 0x100968d8, 80 bytes: '#' into the new image for every second cell of the area (x1 + 1 to x2 + 1, y1 + 1 to y2 + 1, step 2), at (x >> 1, y >> 1).
void LLCC_AddNewAreaToImage(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    uint8_t* image = NewImage();
    for (int32_t y = y1 + 1; y <= y2 + 1; y += 2) {
        for (int32_t x = x1 + 1; x <= x2 + 1; x += 2) {
            image[(x >> 1) * kImageSide + (y >> 1)] = '#';
        }
    }
}

// 0x10096958, 140 bytes: fills the new image from the corner (0x41, 0) with '#' and answers 0 when a ' ' or '@' is left inside (index row * 0x43 + column,
// the outer loop on the column, 1 to 0x40 each way), else 1.
int32_t LLCC_SoIsItLegal() {
    uint8_t* image = NewImage();
    LLCC_FloodFill(0x41, 0, ' ', '#');
    for (int32_t outer = 1; outer < 0x41; ++outer) {
        for (int32_t inner = 1; inner < 0x41; ++inner) {
            const uint8_t value = image[inner * kImageSide + outer];
            if (value == ' ' || value == '@') return 0;
        }
    }
    return 1;
}

// 0x10096a0c, 8 bytes.
int32_t LLCC_IsClearLandLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    return 1;
}

// 0x10096a44, 340 bytes: counts the valid cells of the area (rows y1 to y2, columns x1 to x2) and those that are hoarding (a track X cell through its
// parent) or whose map cell is flagged 0x40; answers whether the two counts differ.
bool LLCC_IsPatrolAreaLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int32_t valid = 0;
    int32_t blocked = 0;
    for (int32_t y = y1; y <= y2; ++y) {
        for (int32_t x = x1; x <= x2; ++x) {
            if (CMap::IsXYValid(x, y) != 0) {
                CMapCell* mapCell = MapCellOfId(CellId(x, y));
                CTrackCell* cell = mapCell->GetTrackCellEquiv();
                if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
                if (cell->IsHoarding() != 0) {
                    ++blocked;
                } else if (mapCell->IsFlag(0x40) != 0) {
                    ++blocked;
                }
                ++valid;
            }
        }
    }
    return blocked != valid;
}
