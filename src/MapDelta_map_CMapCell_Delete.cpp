// CMapCell::Delete (main app, SimTheme Park 2.04b3; unit v5211), 0x1009d0f4, 1860 bytes. Takes the cell back to its start state (mode 0, no
// neighbours, no flags), by the cell's mode: one jump table of 22 entries (modes above 0x15 do nothing).
//   path   : a counter (mOverlapCounter) counts the deletions; at zero (or `a` and `b` both 0, or the cell system's flag) every neighbour
//            link is cut on both sides and the objects (queues) the neighbours are part of recompute their queue.
//   queue  : the same counter, kept at -1 when the cell is a bend/junction; the value of the object on the cell is paid back to the bank.
//   track  : the track cell of the same place loses its mesh instance.
#include <cstdint>

#include "CellBase.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

// Halfwords the original compares ids with, both 0 and never written (names invented).
uint16_t gDeleteNoParent = 0;   // 0x1029e226
uint16_t gDeleteNoThing = 0;    // 0x1029e228

namespace {

const char kThingZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kNoModify[] = "Removing path cell with no neighbours but NOMODIFY set\n";

// The jump table 0x1029e22c: 0 nothing, 1 the path code, 2 the queue code, 3 the track code.
// (4 is the track code from the point after the track cell's mesh instance is cleared.)
const uint8_t kHandler[22] = {0, 1, 0, 2, 4, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3};

CMapCell* CellOf(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

// The thing of the entry; entry 0 must be free (a release assert that does nothing here, but the original calls it).
void* ThingOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZero);
    return gThingArray[id].mThing;
}

// Pays back the value of the ride (object) on the cell the parent id names, when that cell has a mode.
void PayBack(CMapCell* self) {
    CMapCell* parent = CellOf(self->mParentID);
    if (parent->mType == 0) return;
    const uint16_t id = parent->GetObjectThingId().mIndex;
    CObject* object = static_cast<CObject*>(ThingOf(id));
    const int32_t percentage = object->GetResidualValuePercentage();
    const int32_t cost = LLCC_SystemGetQueueCost();
    const uint32_t product = static_cast<uint32_t>(percentage) * static_cast<uint32_t>(cost);
    const uint32_t amount = static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851fu) * product) >> 32) >> 5;
    gGameWorld->GetBankAccount()->MakeDeposit(amount);
}

void ClearCell(CMapCell* self) {
    self->RemoveNeighbour(0xff);
    self->ModifyDirection(0);
    self->mFlags = 0;
    self->mParentID = 0;
    self->ModifyType(static_cast<CELL_MODES>(0));
    self->SetHFieldTile();
}

}  // namespace

void CMapCell::Delete(int32_t a, int32_t b) {
    if (static_cast<int32_t>(mType) == 1 && (mFlags & 0x20) != 0) {
        if (NumberOfNeighbours() == 0) {
            CTRACE(6, kNoModify);
            mFlags = static_cast<uint16_t>(mFlags & 0xffdf);
        }
    }
    if ((mFlags & 0x20) != 0 && gLLCC47E4 == 0) return;
    const uint32_t type = static_cast<uint32_t>(mType);
    if (type > 0x15) return;
    switch (kHandler[type]) {
    case 1: {
        if (a == 0 && b == 0) mOverlapCounter = -1;
        else mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1);
        if (mOverlapCounter >= 0 && gLLCC47E4 == 0) return;
        mOverlapCounter = 0;
        uint8_t dir = 1;
        do {
            if ((GetNeighbours() & dir) != 0) {
                RemoveNeighbour(dir);
                CMapCell* n = GetNeighbouringCell(dir);
                if (n != nullptr) {
                    n->RemoveNeighbour(OPPOSITE(dir));
                    n->SetHFieldTile();
                    if (static_cast<int32_t>(mType) == 1 && static_cast<int32_t>(n->mType) == 9 &&
                        (dir == 1 || dir == 4 || dir == 0x10 || dir == 0x40)) {
                        const uint16_t parent = n->mParentID;
                        if (parent != gDeleteNoParent) {
                            CMapCell* other = CellOf(parent);
                            const uint16_t id = other->GetObjectThingId().mIndex;
                            if (id != gDeleteNoThing) {
                                CThing* thing = static_cast<CThing*>(ThingOf(id));
                                if (thing->mKind == 3) static_cast<CObject*>(ThingOf(id))->RecalculateBackOfQueue();
                            }
                        }
                    }
                }
            }
            dir = ADD_DIRECTION(dir, 2);
        } while (dir != 1);
        ClearCell(this);
        break;
    }
    case 2: {
        if (a == 0 && b == 0) {
            mOverlapCounter = -1;
        } else {
            uint8_t nb = GetNeighbours();
            if (NumberOfHVNeighbours() == 2) {
                const bool low = (nb & 1) != 0;
                const bool keep = (low && (nb & 4)) || (low && (nb & 0x40)) || ((nb & 0x10) && (nb & 4)) || ((nb & 0x10) && (nb & 0x40));
                if (!keep) nb = 0;
            } else {
                nb = 0;
            }
            if (nb != 0) mOverlapCounter = -1;
            else mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1);
        }
        if (gLLCC47E4 != 0) {
            mOverlapCounter = 0;
            if (static_cast<int32_t>(mType) != 9) PayBack(this);
            ClearCell(this);
            return;
        }
        if (mOverlapCounter >= 0) return;
        mOverlapCounter = 0;
        if (static_cast<int32_t>(mType) != 9) PayBack(this);
        uint8_t dir = 1;
        do {
            if ((GetNeighbours() & dir) != 0) {
                RemoveNeighbour(dir);
                CMapCell* n = GetNeighbouringCell(dir);
                if (n != nullptr) {
                    n->RemoveNeighbour(OPPOSITE(dir));
                    n->SetHFieldTile();
                }
            }
            dir = ADD_DIRECTION(dir, 4);
        } while (dir != 1);
        ClearCell(this);
        break;
    }
    case 3: {
        GetTrackCellEquiv()->mMeshInstance = 0;
        ClearCell(this);
        break;
    }
    case 4:
        ClearCell(this);
        break;
    default:
        break;
    }
}
