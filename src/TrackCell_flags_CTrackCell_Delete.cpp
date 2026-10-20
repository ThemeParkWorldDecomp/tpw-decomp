// CTrackCell::Delete 0x100a40c4 (3,480 bytes): takes a track, kart, water or hoarding cell out of the track system.
#include <cstdint>
#include <cstdlib>

#include "TrackCell.h"
#include "ObjectParts.h"
#include "LLCellSystem.h"
#include "World.h"
#include "ThingReceivers.h"
#include "ActionRecorder.h"
#include "Debug.h"

namespace {

// The cell of a parent id: id - 1 cells into the world's track cells (0x1102d8), computed in 32 bits (id 0 lands one cell before).
inline CTrackCell* TrackCellOf(uint16_t id) {
    return gGameWorld->mTrackCells + (static_cast<int32_t>(id) - 1);
}

inline bool IsTrackXType(int32_t t) { return t == 0xc || t == 0x11; }
inline bool IsRideType(int32_t t) { return t == 0xb || t == 0xd || t == 0x10 || t == 0x12 || t == 0x19; }

// Whether the cell, or the cell its track part belongs to, is a ride cell of kind 0xd or 0x12 (inlined in the original).
inline bool IsRideEnd(CTrackCell* n) {
    if (IsTrackXType(n->mType)) {
        CTrackCell* p = TrackCellOf(n->mParentID);
        if (p != nullptr && (p->mType == 0xd || p->mType == 0x12)) return true;
    }
    return n->mType == 0xd || n->mType == 0x12;
}

}  // namespace

// The jump table at 0x1029e344 (15 words, mode - 0xb): 1 where the case body at 0x100a4240 runs, 0 where it goes to 0x100a4ad8.
static const uint8_t kCaseBody[15] = {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1};

void CTrackCell::Delete(int32_t a, int32_t b, bool c) {
    uint16_t first = 0;   // r29
    uint16_t last = 0;    // r23
    uint32_t cost = 0;    // r25
    uint8_t dir;
    uint8_t nb;
    bool corner;
    CTrackCell* n;

    if (mType == 0x10) {
        cost = LLCC_SystemGetWaterTrackCost();
    } else if (mType == 0xb) {
        cost = LLCC_SystemGetKartTrackCost();
    }

    if (IsTrackXType(mType)) {
        TrackCellOf(mParentID)->Delete(a, b, true);
        goto L4ad8;
    }

    if (CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY)) {
        first = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCFirstBlockX) +
                                      (static_cast<uint32_t>(gLLCCFirstBlockY) << 7) + 1);
    }
    if (CMap::IsXYValid(gLLCCE034, gLLCCE038)) {
        last = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCE034) +
                                     (static_cast<uint32_t>(gLLCCE038) << 7) + 1);
    }

    {
        const uint32_t index = static_cast<uint32_t>(mType) - 0xb;
        if (index > 0xe || kCaseBody[index] == 0) goto L4ad8;
    }

    if (a == 0 && b == 0 && mCellId != first) {
        mOverlapCounter = -1;
        goto L4560;
    }
    mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1);
    if (a == -1 && b == -1) {
        switch (GetDirection()) {
            case 1:
            case 0x10:
                RemoveNeighbour(4);
                RemoveNeighbour(0x40);
                break;
            case 4:
            case 0x40:
                RemoveNeighbour(1);
                RemoveNeighbour(0x10);
                break;
            default:
                break;
        }
        SetHFieldTile();
        goto L4560;
    }
    if (NumberOfDirections() >= 2) {
        if (a == -2) RemoveDirection(4);
        if (a == 2) RemoveDirection(0x40);
        if (b == -2) RemoveDirection(0x10);
        if (b == 2) RemoveDirection(1);
        if (std::abs(a) == 2) {
            RemoveNeighbour(4);
            RemoveNeighbour(0x40);
        }
        if (std::abs(b) == 2) {
            RemoveNeighbour(0x10);
            RemoveNeighbour(1);
        }
        SetHFieldTile();
        goto L4560;
    }
    if (mCellId == first || mCellId == last) goto L4560;

    corner = false;
    nb = GetNeighbours();
    if (NumberOfHVNeighbours() == 2) {
        const bool side = (nb & 4) != 0 || (nb & 0x40) != 0;
        corner = ((nb & 1) != 0 && side) || ((nb & 0x10) != 0 && side);
    }
    if (a == -2) RemoveNeighbour(4);
    if (a == 2) RemoveNeighbour(0x40);
    if (b == -2) RemoveNeighbour(0x10);
    if (b == 2) RemoveNeighbour(1);
    if (corner && NumberOfNeighbours() == 1) {
        ModifyDirection(OPPOSITE(GetNeighbours()));
    }
    SetHFieldTile();

L4560:
    if (mOverlapCounter < 0 && mCellId != first && mCellId != last) goto L45c4;
    if (mOverlapCounter < 0 && gLLCC47E0 != 0) goto L45c4;
    if (gLLCC47E4 == 0) goto L4ad8;

L45c4:
    mOverlapCounter = 0;
    if (mType != 0x19 && gActionRecorder.mRecording == 0) {
        CMapCell* mapCell = TrackCellOf(mParentID)->GetMapCellEquiv();
        auto thingId = mapCell->GetObjectThingId();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr,
                       "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        const int32_t percentage = static_cast<CObject*>(gThingArray[thingId.mIndex].mThing)->GetResidualValuePercentage();
        CBankAccount* bank = gGameWorld->GetBankAccount();
        const uint32_t product = cost * static_cast<uint32_t>(percentage);
        // x / 100 as the compiler writes it: mulhwu by 0x51eb851f, then a shift of 5.
        bank->MakeDeposit(static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851fu) * product) >> 32) >> 5);
    }

    dir = 1;
    do {
        if ((GetNeighbours() & dir) != 0) {
            RemoveNeighbour(dir);
            n = GetNeighbouringCell(dir, 2);
            if (n != nullptr) {
                n->RemoveNeighbour(OPPOSITE(dir));
                n->SetHFieldTile();
                if (n->NumberOfNeighbours() == 1) {
                    uint8_t neighbours = n->GetNeighbours();
                    if (n->GetDirection() != neighbours) {
                        neighbours = OPPOSITE(n->GetNeighbours());
                        if (n->GetDirection() != neighbours) {
                            n->ModifyDirection(OPPOSITE(n->GetNeighbours()));
                        }
                    }
                }
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);

    if ((mFlags & 0x10) != 0) {
        const int32_t id = static_cast<int32_t>(mCellId) - 1;
        LLCR_DeleteRide(id & 0x7f, static_cast<int32_t>(static_cast<uint32_t>(id) >> 7), 1);
    }
    ModifyDirection(0);
    mFlags = 0;
    ModifyType(static_cast<CELL_MODES>(0));
    if (mType != 0x19) {
        mParentID = 0;
        mSegmentNumber = -1;
    }
    SetHFieldTile();

    n = GetNeighbouringCell(gMapDeltaPlusY, 1);
    if (n != nullptr && n->mParentID == mCellId) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->SetOverlapCounter(0);
        if (mType != 0x19) n->mParentID = 0;
        if (c) n->SetHFieldTile();
    }
    n = GetNeighbouringCell(gMapDeltaPlusX, 1);
    if (n != nullptr && n->mParentID == mCellId) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->SetOverlapCounter(0);
        if (mType != 0x19) n->mParentID = 0;
        if (c) n->SetHFieldTile();
    }
    n = GetNeighbouringCell(gMapDeltaPlusXPlusY, 1);
    if (n != nullptr && n->mParentID == mCellId) {
        n->RemoveNeighbour(0xff);
        n->ModifyDirection(0);
        n->mFlags = 0;
        n->ModifyType(static_cast<CELL_MODES>(0));
        n->SetOverlapCounter(0);
        if (mType != 0x19) n->mParentID = 0;
        if (c) n->SetHFieldTile();
    }

L4ad8:
    if (mCellId == first && IsRideType(mType) && gLLCC47E0 == 0) {
        dir = 1;
        do {
            if ((GetNeighbours() & dir) == 0) {
                n = GetNeighbouringCell(dir, 2);
                if (n != nullptr) {
                    if (IsRideEnd(n) && n->mParentID == mParentID) {
                        ModifyDirection(OPPOSITE(dir));
                        ModifyNeighbours(dir);
                        SetHFieldTile();
                    }
                    if (!IsRideType(n->mType)) {
                        RemoveNeighbour(dir);
                        SetHFieldTile();
                    }
                }
            }
            dir = ADD_DIRECTION(dir, 4);
        } while (dir != 1);
        dir = 1;
        if (NumberOfNeighbours() == 1) {
            RemoveDirection(0xff);
            do {
                if ((GetNeighbours() & dir) != 0) {
                    n = GetNeighbouringCell(dir, 2);
                    if (n != nullptr && IsRideEnd(n) && n->mParentID == mParentID) {
                        ModifyDirection(OPPOSITE(dir));
                    }
                }
                dir = ADD_DIRECTION(dir, 4);
            } while (dir != 1);
        }
    }
    if (mType != 0 && mMeshInstance == 0) SetHFieldTile();
}
