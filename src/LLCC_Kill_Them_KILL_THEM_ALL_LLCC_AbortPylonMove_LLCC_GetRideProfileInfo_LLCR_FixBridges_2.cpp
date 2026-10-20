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

namespace {

// The track cell of a (1 based) cell id; the Mac subtracts one in 32 bits, so id 0 lands one cell before the grid.
CTrackCell* TrackCellAt(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x1102d8 +
                                         static_cast<int32_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

void Step(uint8_t dir, int32_t& x, int32_t& y) {
    switch (dir) {
        case 0x10: y += 2; break;
        case 4: x += 2; break;
        case 1: y -= 2; break;
        case 0x40: x -= 2; break;
        default: break;
    }
}

}  // namespace

// 0x1008fa34 (2020 bytes): walks the ride's track cell by cell from its start, turning the bridge cells' flags to match their
// neighbours (and starting or stopping the ride's flashing / animation); answers 1 when the walk stopped at the start of another
// ride's track or the first cell is not a track.
uint8_t LLCR_FixBridges(CObject* ride, bool a, bool b, bool c) {
    tag_sRideAnimSet** const sets = gRideAnimSets;
    uint8_t found = 0;
    uint32_t result = 0;

    uint16_t id = static_cast<uint16_t>(ride->mBase.mCellX + ride->mBase.mCellY * 128 + 1);
    int32_t x = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(id) - 1) & 0x7f);
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(id) - 1) >> 7);
    uint8_t dir = LLCR_OffsetStartOfTrack(&x, &y, ride->mRotation);
    gLLCCE0CC = dir;

    if (!TrackCellAt(x, y)->IsTrack()) {
        result = 1;
        found = 1;
    }

    do {
        CTrackCell* cell = TrackCellAt(x, y);
        if (cell->IsJustTrack()) {
            if (cell->IsCorner()) {
                uint8_t r17 = OPPOSITE(dir);
                if ((r17 & cell->GetNeighbours()) == 0) r17 = dir;
                dir = static_cast<uint8_t>(r17 ^ cell->GetNeighbours());
            } else if (cell->IsBridge()) {
                CTrackCell* n19 = cell->GetNeighbouringCell(dir, 2);
                CTrackCell* n18 = cell->GetNeighbouringCell(OPPOSITE(dir), 2);
                if (cell->NumberOfNeighbours() == 2) {
                    if (n19->IsBridge() && n18->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    } else if (!n19->IsBridge() && !n18->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(1);
                    } else if (n19->IsBridge() && !n18->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(2);
                    } else if (!n19->IsBridge() && n18->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(8);
                    }
                }

                bool fixA = false;
                if (!n18->IsTrack()) {
                    fixA = true;
                } else if (n18->IsTrack()) {
                    if (cell->mParentID != n18->mParentID) fixA = true;
                }
                if (!fixA) {
                    if (n19->IsTrack() && cell->NumberOfNeighbours() == 1 && (cell->GetNeighbours() & dir) != 0) fixA = true;
                }
                if (fixA) {
                    if (cell->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        if (n19->IsTrack()) cell->SetFlag(8);
                        else cell->SetFlag(2);
                    }
                    if (n19->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    }
                }

                bool fixB = false;
                if (!n19->IsTrack()) {
                    fixB = true;
                } else if (n19->IsTrack()) {
                    if (cell->mParentID != n19->mParentID) fixB = true;
                }
                if (!fixB) {
                    if (n18->IsTrack() && cell->NumberOfNeighbours() == 1) {
                        uint8_t opp = OPPOSITE(dir);
                        if ((cell->GetNeighbours() & opp) != 0) fixB = true;
                    }
                }
                if (fixB) {
                    if (cell->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        if (n19->IsTrack()) cell->SetFlag(8);
                        else cell->SetFlag(2);
                    }
                    if (n18->IsBridge()) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    }
                }

                cell->SetHFieldTile();
                if (b && cell->mMeshInstance != 0) {
                    if (c) SetCurRideAnim(sets[cell->mMeshInstance], 5, 0, 7, 1.0f, 0);
                    else SetCurRideAnim(sets[cell->mMeshInstance], 0xc, 0, 7, 1.0f, 0);
                }
            }

            if (!cell->IsCorner() && (cell->GetNeighbours() & dir) == 0) {
                found = 1;
                if (cell->mMeshInstance != 0) {
                    tag_sRideAnimSet* set = sets[cell->mMeshInstance];
                    if (a) {
                        if ((set->mFlags & 0x4000000) == 0) StartFlashingObject(set);
                    } else {
                        if ((set->mFlags & 0x4000000) != 0) StopFlashingObject(set);
                    }
                }
            }

            CTrackCell* next = cell->GetNeighbouringCell(dir, 2);
            if (next != nullptr && next->IsTrackStartBlock() && next->mParentID == cell->mParentID) {
                uint8_t opp = OPPOSITE(dir);
                if ((next->GetNeighbours() & opp) != 0) {
                    result = 1;
                    found = 1;
                    Step(dir, x, y);
                }
            }
        }
        if (!found) Step(dir, x, y);
    } while (!found);
    return static_cast<uint8_t>(result);
}
