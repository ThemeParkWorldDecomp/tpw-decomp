#include <cstdint>
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "World.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "RideDB.h"
#include "UIInterface.h"
#include "ThingReceivers.h"
#include "BMP_InsideArea.h"
#include "ObjectParts.h"
#include "analyser.h"
#include "Debug.h"
#include "ppc_math.h"

// The placement record (LLCR_PRR) is only read by Mac offset: +0 x, +4 y, +8 track x, +0xc track y, +0x10 direction (byte).
static int32_t PrrWord(LLCR_PRR* p, int32_t offset) { return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(p) + offset); }
static uint8_t PrrDirection(LLCR_PRR* p) { return *(reinterpret_cast<uint8_t*>(p) + 0x10); }

static uint8_t* MapCellAt(uint16_t id) {
    return reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44;
}

static uint8_t* TrackCellAt(uint16_t id) {
    return reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000;
}

// 0x1008c128.
CCellBase* LLCR_StartQueue(LLCR_PRR* placement, int32_t* x, int32_t* y) {
    if (!CMap::IsXYValid(PrrWord(placement, 0), PrrWord(placement, 4))) return nullptr;
    uint16_t id = static_cast<uint16_t>(PrrWord(placement, 0) + PrrWord(placement, 4) * 128 + 1);
    CCellBase* first = reinterpret_cast<CCellBase*>(MapCellAt(id));
    first->ModifyNeighbours(OPPOSITE(PrrDirection(placement)));
    *x = PrrWord(placement, 0);
    *y = PrrWord(placement, 4);
    switch (PrrDirection(placement)) {
        case 0x10: *y = *y - 1; break;
        case 4: *x = *x - 1; break;
        case 1: *y = *y + 1; break;
        case 0x40: *x = *x + 1; break;
        default: break;
    }
    if (!CMap::IsXYValid(*x, *y)) return nullptr;
    uint16_t id2 = static_cast<uint16_t>(*x + *y * 128 + 1);
    CMapCell* cell = reinterpret_cast<CMapCell*>(MapCellAt(id2));
    if (cell->ValidTypeChange(static_cast<CELL_MODES>(3))) cell->ModifyDirection(PrrDirection(placement));
    return cell;
}

// 0x1008c320. A track kind other than 1 or 2 leaves the first mode argument as the placement's address (low 32 bits). A rotation other
// than 0, 0x5a, 0xb4 or 0x10e leaves the coordinates unset on the Mac (callee-saved garbage): they are taken as 0 here.
CTrackCell* LLCR_StartTrack(LLCR_PRR* placement, int32_t* x, int32_t* y, int32_t track) {
    int32_t px = PrrWord(placement, 8), py = PrrWord(placement, 0xc);
    CELL_MODES mode = static_cast<CELL_MODES>(static_cast<int32_t>(reinterpret_cast<uintptr_t>(placement)));
    if (track == 2) mode = static_cast<CELL_MODES>(0x12);
    else if (track >= 1 && track < 2) mode = static_cast<CELL_MODES>(0xd);
    int32_t r30 = px, r29 = py;
    int32_t r25 = 0, r26 = 0, r27 = 0, r28 = 0;
    int32_t endX = 0, endY = 0;
    switch (gLLCRPlaceRotation) {
        case 0:
            r30 += 4; r29 += 1;
            endX = r30; r27 = r29; r25 = r29;
            r26 = r30 - 4; r28 = r30 - 4;
            endY = r27;
            endX -= 2;
            gLLCCE0CC = 4;
            break;
        case 0x5a:
            r29 -= 5; r30 += 1;
            endY = r29; r28 = r30; r26 = r30;
            r25 = r29 + 5; endX = r28; r27 = r29 + 4;
            endY += 2;
            gLLCCE0CC = 1;
            break;
        case 0xb4:
            r30 -= 5; r29 -= 2;
            endX = r30; r27 = r29; r25 = r29;
            r26 = r30 + 5; endY = r27; r28 = r30 + 4;
            endX += 2;
            gLLCCE0CC = 0x40;
            break;
        case 0x10e:
            r29 += 4; r30 -= 2;
            endY = r29; r28 = r30; r26 = r30;
            r25 = r29 - 4; endX = r28; r27 = r29 - 4;
            endY -= 2;
            gLLCCE0CC = 0x10;
            break;
        default: break;
    }
    LLCC_DrawTrackLine(mode, r28, r27, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x80), r28, r27, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x82), r28, r27, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x83), r28, r27, &endX, &endY);
    if (CMap::IsXYValid(r28, r27)) {
        gLLCCFirstBlockX = r28;
        gLLCCFirstBlockY = r27;
    }
    if (!CMap::IsXYValid(r30, r29)) return nullptr;
    *x = r30;
    *y = r29;
    uint16_t startId = static_cast<uint16_t>(r26 + r25 * 128 + 1);
    uint16_t endId = static_cast<uint16_t>(*x + *y * 128 + 1);
    gLLCCDB44 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(TrackCellAt(startId)));
    return reinterpret_cast<CTrackCell*>(TrackCellAt(endId));
}
