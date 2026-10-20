// Unit v5629: CTrackCell::ConnectionType (0x100a1594, 4,932 bytes), how a cell of mode `mode` would join this track cell (the cell
// system's placement test). It also owns five cell system words no other file names (see TrackCell.h).
#include <cstdint>
#include "TrackCell.h"

#include "ActionRecorder.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

// gLLCCC20C, gLLCCDB60, gLLCCE0B4, gLLCCE0B8 are defined by other units now.
int32_t gLLCCDB40 = 0;   // 0x102ddb40

namespace {

// The ride rule of one map cell: a ride part (mode 4, 9 or 10) whose ride ranks below gLLCCE0B8 forbids the placement (answers false).
// The ride's rank (kRideSlot_OverwritePriority, the record's word 0x54) is left in gLLCCE0B4 when the cell's thing is an object.
bool RideAllows(const CMapCell* cell) {
    const int32_t type = static_cast<int32_t>(cell->mType);
    if (type != 4 && type != 9 && type != 10) return true;
    const CMapCell* parent = &gGameWorld->mMapCells[static_cast<int32_t>(cell->mParentID) - 1];
    const uint16_t id = parent->GetObjectThingId().mIndex;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    const CThing* thing = static_cast<const CThing*>(gThingArray[id].mThing);
    if (thing->mKind == 3) {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ride;
        ride.mIndex = static_cast<const CObject*>(gThingArray[id].mThing)->mObjectType;
        gLLCCE0B4 = gRideDatabase.GetRideInfo(ride)->Value(kRideSlot_OverwritePriority).mInteger;
    }
    return !(gLLCCE0B8 > gLLCCE0B4);
}

bool IsSpecial(CMapCell* cell) {
    return cell->IsRide() != 0 || cell->IsFreeform() != 0 || cell->IsExit() != 0 || cell->IsEntrance() != 0 ||
           static_cast<int32_t>(cell->mType) == 0x15;
}

// A track neighbour of another segment (a track-X cell stands for the cell of its parent id) blocks the cell.
bool OtherSegment(CTrackCell* cell) {
    if (static_cast<int32_t>(cell->mType) == 0) return false;
    if (cell->IsTrackX() != 0) cell = &gGameWorld->mTrackCells[static_cast<int32_t>(cell->mParentID) - 1];
    if (cell->IsTrack() != 0 && cell->mParentID != gLLCCDB68) {
        gLLCCC214 = 1;
        return true;
    }
    return false;
}

}  // namespace

int32_t CTrackCell::ConnectionType(CELL_MODES modeArg, int32_t a, uint8_t b) {
    const int32_t mode = static_cast<int32_t>(modeArg);
    bool blocked = false;
    int32_t cost = 0;

    if (mode == 0x39) return gLLCCC214 != 0 ? 1 : 0;

    CMapCell* m0 = GetMapCellEquiv();
    MapDelta d1 = gMapDeltaPlusX;
    CMapCell* m1 = m0->GetNeighbouringCell(&d1, 1);
    MapDelta d2 = gMapDeltaPlusXPlusY;
    CMapCell* m2 = m0->GetNeighbouringCell(&d2, 1);
    MapDelta d3 = gMapDeltaPlusY;
    CMapCell* m3 = m0->GetNeighbouringCell(&d3, 1);

    if (IsSpecial(m0) || IsSpecial(m1) || IsSpecial(m2) || IsSpecial(m3)) blocked = true;

    if (m0->IsFlag(0x40) != 0 || m1->IsFlag(0x40) != 0 || m2->IsFlag(0x40) != 0 || m3->IsFlag(0x40) != 0) return 1;

    // The original tests m2 twice and never m3.
    if (!RideAllows(m0)) return 0;
    if (!RideAllows(m1)) return 0;
    if (!RideAllows(m2)) return 0;
    if (!RideAllows(m2)) return 0;

    if (mode == 0xb) cost = static_cast<int32_t>(LLCC_SystemGetKartTrackCost());
    if (mode == 0x10) cost = static_cast<int32_t>(LLCC_SystemGetWaterTrackCost());
    if (mode == 0xb || mode == 0x10) {
        CTrackCell* t1 = GetNeighbouringCell(gMapDeltaPlusX, 1);
        CTrackCell* t2 = GetNeighbouringCell(gMapDeltaPlusXPlusY, 1);
        CTrackCell* t3 = GetNeighbouringCell(gMapDeltaPlusY, 1);
        if (OtherSegment(t1)) blocked = true;
        if (OtherSegment(t2)) blocked = true;
        if (OtherSegment(t3)) blocked = true;
    }
    if (gLLCCC210 != 0) gLLCCC214 = 1;
    if (a == 0 && gLLCCC214 != 0) return 1;

    int32_t type = static_cast<int32_t>(mType);
    if (type == 0x19 && mode == 0x19) {
        const uint32_t index = static_cast<uint32_t>(mCellId) - 1;
        const int32_t x = static_cast<int32_t>(index & 0x7f);
        const int32_t y = static_cast<int32_t>(index >> 7);
        if (x == gHoardingRight && y == gHoardingBottom) return 1;
        if (x == gHoardingLeft && y == gHoardingTop) return 1;
        return 0;
    }
    if (mode == 0x19) {
        if (type == 0x19) {   // unreachable: the test above took mode and type 0x19
            if (b != OPPOSITE(mDirection)) return 0;
            gLLCCC214 = 1;
            return 1;
        }
        if (type == 0 || type == 7) return 0;
        gLLCCC214 = 1;
        return 1;
    }
    if (mode == type && mParentID == gLLCCDB68) {
        if (gLLCCC20C == 0 || NumberOfNeighbours() == 1 || NumberOfNeighbours() == 3) gLLCCC20C = 1;
    }
    if (mode == static_cast<int32_t>(mType) && gTrackNbrOn485C == 0 && gLLCC4858 == 0 && IsStraight() == 0 &&
        NumberOfNeighbours() != 1) {
        if (a == 0) gLLCCC210 = 1;
        return 1;
    }

    GetMapCellEquiv();   // the answer is unused
    bool costs;
    type = static_cast<int32_t>(mType);
    if (type == 0x19 || type == 7 || IsFlag(0x40) != 0 || mode == 0) {
        costs = true;
    } else {
        type = static_cast<int32_t>(mType);
        costs = type == 0 || mode == type;
    }
    if (costs && static_cast<int32_t>(mType) != mode && gTrackCostFree == 0) LLCC_SystemAddToSessionCost(cost);

    if (gActionRecorder.mRecording == 0 && mode != 0xe && mode != 0x13) {
        const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        if (LLCC_SystemGetSessionCost() > balance) {
            if (a == 0) {
                gLLCCC214 = 1;
                LLCM_SetIllegalPlacement(1);
            }
            gLLCCC204 = 1;
            return 1;
        }
    }

    type = static_cast<int32_t>(mType);
    if (gLLCC4858 != 0 && type != 0xb && type != 0x10 && (mode == 0xb || mode == 0x10) &&
        (m0->IsQueue() != 0 || m1->IsQueue() != 0 || m2->IsQueue() != 0 || m3->IsQueue() != 0 || m0->IsFlag(0x20) != 0 ||
         m1->IsFlag(0x20) != 0 || m2->IsFlag(0x20) != 0 || m3->IsFlag(0x20) != 0)) {
        if (a == 0) gLLCCC210 = 1;
        return 1;
    }

    type = static_cast<int32_t>(mType);
    if (type == 0 && mode != 0xe && mode != 0x13 && !blocked) {
        if (a == 0) gLLCCC20C = 0;
        return 0;
    }

    if (mode == type || (mode == 0xb && type == 0xd) || (mode == 0x10 && type == 0x12)) {
        if ((gTrackNbrOn485C != 0 && gLLCCDB60 != 0) || gLLCC4858 != 0) {
            if (type == 0xb || type == 0x10) {
                if (b != mDirection && NumberOfNeighbours() == 2) {
                    if (a == 0) gLLCCC210 = 1;
                    return 1;
                }
                if (IsCorner() != 0) {
                    if (a == 0) gLLCCC210 = 1;
                    return 1;
                }
            }
            type = static_cast<int32_t>(mType);
            if ((type == 0xd || type == 0x12) && b != mDirection) {
                if (a == 0) gLLCCC210 = 1;
                return 1;
            }
            if (IsBridge() != 0 && b != 0 && b != mDirection && (gLLCC4858 != 0 || gTrackNbrOn485C != 0)) {
                if (a == 0) gLLCCC210 = 1;
                return 1;
            }
            if (b != OPPOSITE(mDirection) && mParentID == gLLCCDB68 && IsFlag(0x10) != 0 && b != mDirection) {
                if (a == 0) gLLCCC210 = 1;
                return 1;
            }
            gLLCCC200 = 1;
            return 0xb;
        }
        if (IsBridge() != 0 && b != 0 && b != mDirection && (gLLCC4858 != 0 || gTrackNbrOn485C != 0)) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
    }

    if (static_cast<int32_t>(mType) == 0xb && mode == 0xb && b != OPPOSITE(mDirection) && mParentID == gLLCCDB68) {
        if (IsFlag(0x10) != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        if (IsBridge() != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        return 0;
    }
    type = static_cast<int32_t>(mType);
    if (type == 0x12 && mode == 0x10 && b == mDirection) {
        if (a == 0) gLLCCC210 = 1;
        return 0;
    }
    if (type == 0x10 && mode == 0x10 && b != OPPOSITE(mDirection) && mParentID == gLLCCDB68) {
        if (IsFlag(0x10) != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        if (IsBridge() != 0 && b != mDirection) {
            if (a == 0) gLLCCC210 = 1;
            return 1;
        }
        return 0;
    }
    if (static_cast<int32_t>(mType) == 0xd && mode == 0xb && b == mDirection) {
        if (a == 0) gLLCCC210 = 1;
        return 0;
    }
    if (blocked && IsFlag(0x10) == 0) {
        if (a == 0) gLLCCC210 = 1;
        return 1;
    }
    if (mode == 0xf) {
        type = static_cast<int32_t>(mType);
        if ((type == 0xb || type == 0x10) && mParentID == gLLCCDB68) return 0xd;
        if (type == 0x19) return 0xd;
        return 1;
    }
    if (static_cast<int32_t>(mType) == 0xb && (mode == 0xe || mode == 0x13) && mParentID == gLLCCDB68 && IsCrossRoad() == 0) return 9;
    if (static_cast<int32_t>(mType) == 0x10 && (mode == 0x13 || mode == 0xe) && mParentID == gLLCCDB68 && IsCrossRoad() == 0) return 9;
    if (IsCorner() != 0 && gLLCCDB40 == 0) {
        if (a == 0) gLLCCC214 = 1;
        return 1;
    }
    if (IsBridge() != 0) {
        bool fire;
        if (gTrackNbrOn485C != 0 && b != mDirection && b != OPPOSITE(mDirection)) {
            fire = true;
        } else {
            fire = gTrackNbrOn485C == 0;
        }
        if (fire) {
            if (a == 0) gLLCCC214 = 1;
            return 1;
        }
    }
    if (NumberOfNeighbours() == 2 && b != mDirection && b != OPPOSITE(mDirection)) gLLCCC20C = 0;
    if (mode == static_cast<int32_t>(mType) && mParentID == gLLCCDB68 && NumberOfNeighbours() == 3) return 0;
    return 1;
}
