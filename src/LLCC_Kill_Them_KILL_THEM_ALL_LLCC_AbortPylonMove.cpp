// LLCC_AbortPylonMove 0x1008f1a4, LLCC_IsRideMovable 0x1008f330 and LLCC_GetRideBlendInfo 0x1008f574 (unit v5186).
#include <cstdint>

#include "CellBase.h"
#include "LLCellSystem.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "ui_main.h"

int32_t gLLCCE0B4 = -1;   // 0x1029e0b4
int32_t gLLCCE0B8 = -1;   // 0x1029e0b8
uint32_t gLLCCE08C;       // 0x102de08c
LLCCRideBlend gLLCCDF84;  // 0x102ddf84

// 0x1008f1a4 (292 bytes): while a pylon drag is pending, draws the three lines back over its cell, gives the track cell's mesh word
// (+4) the saved value, ends the drag and forgets the cell.
void LLCC_AbortPylonMove() {
    int32_t x = gLLCCE0B4;
    int32_t y = gLLCCE0B8;
    if (x == -1 || y == -1 || gLLCCE090 == 0) return;
    LLCC_DrawLine(static_cast<CELL_MODES>(0x15), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x83), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x81), x, y, &x, &y);
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1].mMeshInstance = gLLCCE08C;
    gLLCCE090 = 0;
    gLLCCE0B4 = -1;
    gLLCCE0B8 = -1;
}

namespace {
CTrackCell* TrackCellOf(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return &gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1];
}
}  // namespace

// 0x1008f330 (528 bytes): the ride's x (byte +4) and y (byte +6) give a cell id; for a ride whose track type is 1 or 2 both the cell at the
// start offset and the cell at the end offset must have exactly one neighbour.
int32_t LLCC_IsRideMovable(CObject* ride) {
    int32_t result = 1;
    int32_t type = ride->GetDBInfo()->Value(kRideSlot_WhichTrackType).mInteger;
    if (type == 0 || type == 3 || type >= 3 || type < 0) return result;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(ride);
    for (int32_t pass = 0; pass < 2; pass++) {
        uint16_t id = static_cast<uint16_t>(bytes[4] + (bytes[6] << 7) + 1);
        int32_t t = static_cast<int32_t>(id) - 1;
        int32_t x = t & 0x7f;
        int32_t y = static_cast<int32_t>(static_cast<uint32_t>(t) >> 7);
        if (pass == 0)
            LLCR_OffsetStartOfTrack(&x, &y, ride->mRotation);
        else
            LLCR_OffsetEndOfTrack(&x, &y, ride->mRotation);
        if (TrackCellOf(x, y)->NumberOfNeighbours() != 1) return 0;
    }
    return result;
}

namespace {
// The cell (x, z) of the shape: ints, width at 0, height at 4, then rows of 20 cells of 8 bytes (the type is the first word).
bool IsBlendCell(const int32_t* shape, int32_t x, int32_t z) {
    if (x < 0 || z < 0 || shape[0] < x || shape[1] < z) return false;
    int32_t type = shape[2 + z * 40 + x * 2];
    return type == 4 || type == 0x17 || static_cast<uint32_t>(type - 9) < 2;
}
}  // namespace

// 0x1008f574 (732 bytes): copies the shape's size into the table, clears 16 x 16 bytes, then flags (x * 16 + z) every cell of the shape
// that is a hoarding-like cell (4, 9, 10, 0x17) without all four neighbours of that kind. Answers the table.
void* LLCC_GetRideBlendInfo(void* shape) {
    const int32_t* s = static_cast<const int32_t*>(shape);
    LLCCRideBlend* table = &gLLCCDF84;
    table->mWidth = s[0];
    table->mHeight = s[1];
    for (int32_t z = 0; z < 16; z++)
        for (int32_t x = 0; x < 16; x++) table->mCells[x * 16 + z] = 0;
    for (int32_t z = 0; z < s[1]; z++) {
        for (int32_t x = 0; x < s[0]; x++) {
            if (!IsBlendCell(s, x, z)) continue;
            int32_t count = 0;
            if (IsBlendCell(s, x, z - 1)) count++;
            if (IsBlendCell(s, x - 1, z)) count++;
            if (IsBlendCell(s, x + 1, z)) count++;
            if (IsBlendCell(s, x, z + 1)) count++;
            if (count != 4) table->mCells[x * 16 + z] = 1;
        }
    }
    return table;
}
