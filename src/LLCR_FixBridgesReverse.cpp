// LLCR_FixBridgesReverse 0x1009024c (2216 bytes): walks a ride's track from its end backwards, one cell (two steps) at a time, and
// rebuilds the bridge pieces' flags; stops when the track runs out or a cell does not continue in the walking direction.
#include <cstdint>

#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "rsse.h"

namespace {

// The bridge pieces' flag choice shared by the four "end" blocks: UnSetFlag(0xf), SetFlag(a or b by IsTrack(far)).
void BridgeEnd(CTrackCell* cell, CTrackCell* far, uint16_t ifTrack, uint16_t otherwise) {
    cell->UnSetFlag(0xf);
    if (far->IsTrack() != 0)
        cell->SetFlag(ifTrack);
    else
        cell->SetFlag(otherwise);
}

}  // namespace

uint8_t LLCR_FixBridgesReverse(CObject* ride, bool a, bool b, bool c) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(ride);
    int32_t x = static_cast<uint16_t>(bytes[4] + (bytes[6] << 7) + 1) - 1 & 0x7f;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(bytes[4] + (bytes[6] << 7) + 1) - 1) >> 7);
    int32_t rotation = *reinterpret_cast<const int32_t*>(bytes + 0x10);
    uint8_t dir = LLCR_OffsetEndOfTrack(&x, &y, rotation);
    gLLCCE0CC = dir;
    bool stop = false;
    if (gGameWorld->mTrackCells[static_cast<uint16_t>(x + (y << 7) + 1) - 1].IsTrack() == 0) stop = true;
    do {
        CTrackCell* cell = &gGameWorld->mTrackCells[static_cast<uint16_t>(x + (y << 7) + 1) - 1];
        if (cell->IsJustTrack() != 0) {
            if (cell->IsCorner() != 0) {
                uint8_t opp = OPPOSITE(dir);
                uint8_t keep = opp;
                if ((opp & cell->GetNeighbours()) == 0) keep = dir;
                dir = keep ^ cell->GetNeighbours();
            } else if (cell->IsBridge() != 0) {
                CTrackCell* far = cell->GetNeighbouringCell(dir, 2);
                CTrackCell* opp = cell->GetNeighbouringCell(OPPOSITE(dir), 2);
                int32_t n = cell->NumberOfNeighbours();
                if (b && cell->mMeshInstance != 0) {
                    if (c)
                        SetCurRideAnim(gRideAnimSets[cell->mMeshInstance], 5, 0, 7, 1.0f, 0);
                    else
                        SetCurRideAnim(gRideAnimSets[cell->mMeshInstance], 0xc, 0, 7, 1.0f, 0);
                }
                if (n == 2) {
                    if (far->IsBridge() != 0 && opp->IsBridge() != 0) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    } else if (far->IsBridge() == 0 && opp->IsBridge() == 0) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(1);
                    } else if (far->IsBridge() != 0 && opp->IsBridge() == 0) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(8);
                    } else if (far->IsBridge() == 0) {
                        if (opp->IsBridge() != 0) {
                            cell->UnSetFlag(0xf);
                            cell->SetFlag(2);
                        }
                    }
                }
                if (opp->IsTrack() == 0 || (opp->IsTrack() != 0 && cell->mParentID != opp->mParentID)) {
                    if (cell->IsBridge() != 0) BridgeEnd(cell, far, 8, 2);
                    if (far->IsBridge() != 0) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    }
                }
                if (opp->IsTrack() != 0 && cell->NumberOfNeighbours() == 1 && (dir & cell->GetNeighbours()) != 0) {
                    if (cell->IsBridge() != 0) BridgeEnd(cell, far, 8, 2);
                    if (far->IsBridge() != 0) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    }
                }
                if (far->IsTrack() == 0 || (far->IsTrack() != 0 && cell->mParentID != far->mParentID)) {
                    if (cell->IsBridge() != 0) BridgeEnd(cell, far, 8, 2);
                    if (opp->IsBridge() != 0) {
                        cell->UnSetFlag(0xf);
                        cell->SetFlag(4);
                    }
                }
                if (far->IsTrack() != 0 && cell->NumberOfNeighbours() == 1) {
                    uint8_t o = OPPOSITE(dir);
                    if ((cell->GetNeighbours() & o) != 0) {
                        if (cell->IsBridge() != 0) BridgeEnd(cell, far, 2, 8);
                        if (opp->IsBridge() != 0) {
                            cell->UnSetFlag(0xf);
                            cell->SetFlag(4);
                        }
                    }
                }
                cell->SetHFieldTile();
            }
            if (cell->IsCorner() == 0 && (cell->GetNeighbours() & dir) == 0) {
                stop = true;
                if (cell->mMeshInstance != 0) {
                    tag_sRideAnimSet* set = gRideAnimSets[cell->mMeshInstance];
                    if (a) {
                        if ((set->mFlags & 0x04000000) == 0) StartFlashingObject(set);
                    } else {
                        if ((set->mFlags & 0x04000000) != 0) StopFlashingObject(set);
                    }
                }
            }
            CTrackCell* next = cell->GetNeighbouringCell(dir, 2);
            if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
                uint8_t o = OPPOSITE(dir);
                if ((next->GetNeighbours() & o) != 0) stop = true;
            }
        }
        if (!stop) {
            switch (dir) {
                case 1: y -= 2; break;
                case 4: x += 2; break;
                case 0x10: y += 2; break;
                case 0x40: x -= 2; break;
                default: break;
            }
        }
    } while (!stop);
    return 0;
}
