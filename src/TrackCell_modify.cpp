// CTrackCell::ModifyType (main app, SimTheme Park 2.04b3, TrackCell.cpp; unit w139), 0x100a0828, 776 bytes.
//
// Changes the mode of a track cell and pays for it. The cost is 0 except for modes 0xb (a kart track) and 0x10 (a water track) while
// gTrackCostFree is 0. Order: ValidTypeChange(mode) refuses (the Mac then answers whether the mode is 0x19, nothing else happens); a mode
// the cell already has only adds 1 to the overlap counter; otherwise, unless a recording runs, the bank balance must cover the cost
// ((balance - cost) >> 31, 32 bit) unless the mode is 0x19, 0 or 7, in which case nothing is stored when it does not. Then the mode is
// stored, the map stamps the cell with a new time stamp, and (for a plain track cell with gLLCC4858 set) the four map cells of the 2 x 2
// block the cell stands for are deleted when all four exist and their modes are 0 or 1. Last the cost is withdrawn (not for modes 0 and 7,
// nor when tracks are free). The Mac answers 1 (done / counted), 0 (refused or unaffordable) or (mode == 0x19) when the change is invalid;
// the declaration returns void and no caller reads it, so the answer is not reproduced.
#include "TrackCell.h"

#include <cstdint>

#include "ActionRecorder.h"
#include "CellBase.h"
#include "ObjectParts.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

void CTrackCell::ModifyType(CELL_MODES mode) {
    const int32_t m = static_cast<int32_t>(mode);
    uint32_t cost = 0;
    if (m == 0xb && gTrackCostFree == 0) {
        cost = LLCC_SystemGetKartTrackCost();
    }
    if (m == 0x10 && gTrackCostFree == 0) {
        cost = LLCC_SystemGetWaterTrackCost();
    }

    if (ValidTypeChange(mode) == 0) {
        return;
    }
    if (mType == mode) {
        ++mOverlapCounter;
        return;
    }

    int32_t canAfford = 1;
    if (gActionRecorder.mRecording == 0) {
        const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        const int32_t left = static_cast<int32_t>(static_cast<uint32_t>(balance) - cost);
        canAfford = (left >> 31) + 1;
    }
    if (m != 0x19 && (canAfford & 0xff) == 0 && m != 0 && m != 7) {
        return;
    }

    mType = mode;
    const uint32_t stamp = gGameWorld->GetMap()->MakeNewTimeStamp();
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id = {mCellId};
    gGameWorld->GetMap()->SetTimeStamp(stamp, id);

    if (IsJustTrack() != 0 && gLLCC4858 != 0) {
        CMapCell* const cell0 = GetMapCellEquiv();
        if (cell0 != nullptr) {
            MapDelta d1 = gMapDeltaPlusX;
            CMapCell* const cell1 = cell0->GetNeighbouringCell(&d1, 1);
            if (cell1 != nullptr) {
                MapDelta d2 = gMapDeltaPlusXPlusY;
                CMapCell* const cell2 = cell0->GetNeighbouringCell(&d2, 1);
                if (cell2 != nullptr) {
                    MapDelta d3 = gMapDeltaPlusY;
                    CMapCell* const cell3 = cell0->GetNeighbouringCell(&d3, 1);
                    if (cell3 != nullptr) {
                        const int32_t t0 = static_cast<int32_t>(cell0->mType);
                        const int32_t t1 = static_cast<int32_t>(cell1->mType);
                        const int32_t t2 = static_cast<int32_t>(cell2->mType);
                        const int32_t t3 = static_cast<int32_t>(cell3->mType);
                        if ((t0 == 0 || t0 == 1) && (t1 == 0 || t1 == 1) && (t2 == 0 || t2 == 1) && (t3 == 0 || t3 == 1)) {
                            cell0->Delete(0, 0);
                            cell1->Delete(0, 0);
                            cell2->Delete(0, 0);
                            cell3->Delete(0, 0);
                        }
                    }
                }
            }
        }
    }

    if (m != 0 && m != 7 && gTrackCostFree == 0) {
        gGameWorld->GetBankAccount()->MakeWithdrawal(cost);
    }
}
