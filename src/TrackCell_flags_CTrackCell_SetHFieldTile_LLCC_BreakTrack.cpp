#include <cstdint>
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "rsse.h"
#include "Debug.h"
#include "World.h"

namespace {

CTrackCell* TrackCellAt(uint16_t id) {
    return &gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1];   // (an id of 0 reads the cell before the grid)
}

// The tail the two removal blocks share: take the bits off the cell, then off the cell beyond.
void BreakBits(CTrackCell* cell, uint8_t bits) {
    cell->RemoveNeighbour(bits);
    cell->SetHFieldTile();
    if (cell->GetOverlapCounter() != 0) cell->DecOverlapCounter();
    CTrackCell* beyond = cell->GetNeighbouringCell(bits, 2);
    if (beyond != nullptr) {
        beyond->RemoveNeighbour(OPPOSITE(bits));
        beyond->SetHFieldTile();
    }
}

}  // namespace

// 0x100a510c (1156 bytes)
CTrackCell* LLCC_BreakTrack(int32_t x, int32_t y, uint8_t dir) {
    uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1);
    CTrackCell* cell = TrackCellAt(id);
    uint8_t bits = cell->GetDirection();
    if (x == gLLCCE034 && y == gLLCCE038) {
        bits = static_cast<uint8_t>(cell->GetNeighbours() & ~bits);
        if (bits != 0) {
            cell->RemoveNeighbour(bits);
            cell->SetHFieldTile();
            if (cell->GetOverlapCounter() != 0) cell->DecOverlapCounter();
            CTrackCell* beyond = cell->GetNeighbouringCell(bits, 2);
            if (beyond != nullptr) {
                beyond->RemoveNeighbour(OPPOSITE(bits));
                beyond->SetHFieldTile();
            }
        }
        return TrackCellAt(id);
    }
    if (dir != 0) bits = dir;
    if (bits != 0) BreakBits(cell, bits);

    const int32_t fx = gLLCCFirstBlockX;
    if (((x + 2 == fx && y == gLLCCFirstBlockY) || (x == fx && y + 2 == gLLCCFirstBlockY) ||
         (x - 2 == fx && y == gLLCCFirstBlockY) || (x == fx && y - 2 == gLLCCFirstBlockY)) &&
        CMap::IsXYValid(fx, gLLCCFirstBlockY) != 0) {
        id = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCFirstBlockX) + (static_cast<uint32_t>(gLLCCFirstBlockY) << 7) + 1);
        CTrackCell* c = TrackCellAt(id);
        static const uint8_t kDirs[4] = {1, 4, 0x10, 0x40};
        static const int32_t kTypes[2] = {0xd, 0x12};
        for (int t = 0; t < 2; ++t) {
            for (int i = 0; i < 4; ++i) {
                if (static_cast<int32_t>(c->GetNeighbouringCell(kDirs[i], 2)->mType) == kTypes[t]) {
                    c->ModifyNeighbours(kDirs[i]);
                    c->SetHFieldTile();
                }
            }
        }
    }
    return TrackCellAt(id);
}

// 0x100a55bc (224 bytes)
uint32_t LLCC_GetTrackMeshAt(int32_t x, int32_t y) {
    const uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1);
    CTrackCell* cell = TrackCellAt(id);
    const int32_t type = static_cast<int32_t>(cell->mType);
    if (type == 0xc || type == 0x11) {
        const uint16_t parent = cell->mParentID;
        if (parent == gLLCCE380) {
            CTRACE(6, "NULL mesh from TrackX... shouldnt happen!!\n");
            return 0;
        }
        cell = TrackCellAt(parent);
    }
    return cell->mMeshInstance;
}
