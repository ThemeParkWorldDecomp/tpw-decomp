#include <cstdint>
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "CellBase.h"
#include "TrackCell.h"
#include "World.h"
#include "TPEngine.h"
#include "rsse.h"

// 0x1008cad8 (756 bytes): walks the track from its end back towards the start, from the cell LLCR_OffsetEndOfTrack gives, two cells at a
// time in the current direction, and sets every cell's ride animation (kind 5 when `flag`, else 0xc). It stops when the cell is not a corner
// and has no neighbour in the direction, or when the neighbouring cell two steps away is a track start block of the same parent that has a
// neighbour back. The original answers r25, which is 0 whenever the loop ends (the header says void).
// The object's bytes: +4 and +6 give the cell (x = low 7 bits, y = the rest of byte4 + byte6 * 128), +0x10 is the rotation.
void LLCC_SetTrackAnimReverse(CObject* object, bool flag) {
    const uint8_t* obj = reinterpret_cast<const uint8_t*>(object);
    uint32_t packed = static_cast<uint16_t>(obj[4] + (obj[6] << 7) + 1);
    int32_t x = static_cast<int32_t>((packed - 1) & 0x7f);
    int32_t y = static_cast<int32_t>((packed - 1) >> 7);
    int32_t rotation = *reinterpret_cast<const int32_t*>(obj + 0x10);
    uint8_t direction = LLCR_OffsetEndOfTrack(&x, &y, rotation);
    int32_t cx = x;
    int32_t cy = y;
    bool keepGoing = true;
    do {
        int32_t index = static_cast<int32_t>(static_cast<uint16_t>(static_cast<uint32_t>(cx) + (static_cast<uint32_t>(cy) << 7) + 1)) - 1;
        char* address = reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + index * 0x28;
        CTrackCell* cell = reinterpret_cast<CTrackCell*>(address);
        cell->GetMapCellEquiv();
        if (cell->IsTrack() == 0) {
            direction = 0;
        } else {
            if (cell->IsCorner() != 0) {
                uint8_t opposite = OPPOSITE(direction);
                if ((opposite & cell->GetNeighbours()) == 0) opposite = direction;
                direction = static_cast<uint8_t>(opposite ^ cell->GetNeighbours());
            }
            uint32_t mesh = cell->mMeshInstance;
            if (mesh != 0) {
                SetCurRideAnim(gRideAnimSets[mesh], flag ? 5 : 0xc, 0, 7, 1.0f, 0);
            }
        }
        if (cell->IsCorner() == 0 && (cell->GetNeighbours() & direction) == 0) keepGoing = false;
        CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
        if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
            uint8_t opposite = OPPOSITE(direction);
            if ((next->GetNeighbours() & opposite) != 0) keepGoing = false;
        }
        switch (direction) {
            case 0x10: cy += 2; break;
            case 4: cx += 2; break;
            case 1: cy -= 2; break;
            case 0x40: cx -= 2; break;
            default: break;
        }
    } while (keepGoing);
}
