// CMapCell::CalculateNeighbours (unit v5632, main app 0x10099998, 5540 bytes; the inventory files it under MapDelta.cpp).
#include <cstddef>
#include <cstdint>
#include "ObjectParts.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "TPCamera.h"
#include "World.h"
#include "MainBalance.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "Debug.h"
#include "rsse.h"
#include "RegionEffectCell.h"
#include "ActionRecorder.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "TPSound.h"
#include "ThingReceivers.h"

int32_t gCalcNeighboursPrevA = 0x80;
int32_t gCalcNeighboursPrevB = 0x80;
uint16_t gCalcNeighboursIdMinusY = 0;
uint16_t gCalcNeighboursIdPlusX = 0;
uint16_t gCalcNeighboursIdPlusY = 0;
uint16_t gCalcNeighboursIdMinusX = 0;
int32_t gCalcNeighboursJoinEntry = 0;

namespace {

// The neighbour at a compass offset (the original copies the offset's two words to the stack and passes their address, flag 1).
CMapCell* Neighbour(CMapCell* cell, const MapDelta& delta) {
    MapDelta copy = delta;
    return cell->GetNeighbouringCell(&copy, 1);
}

// Sound event 0x8b of the ui category at 10 times the cell's (id - 1) & 0x7f, twice (the original computes the same value for both).
void QueueSound(uint16_t cellId) {
    if (gActionRecorder.mRecording != 0) return;
    const int32_t v = static_cast<int32_t>((static_cast<int32_t>(cellId) - 1) & 0x7f);
    const float fv = static_cast<float>(v);
    const int32_t x = static_cast<int32_t>(10.0f * fv);
    const int32_t y = static_cast<int32_t>(10.0f * fv);
    SOUND_SendEvent(0, gSoundBanks[3], 0x8b, x, 0, y);
}

// One of the four straight neighbours of a mode 1 change (path cells): joins it when it has the mode, closes its side corners towards
// path cells that are not queues, then links type 10 and 9 cells by their direction and refreshes a queue that points back.
void Straight(CMapCell* self, CELL_MODES mode, bool flag, const MapDelta& delta, uint8_t mine, uint8_t theirs, const MapDelta& side1,
              uint8_t dir1, uint8_t add1, const MapDelta& side2, uint8_t dir2, uint8_t add2, uint8_t bit10, uint8_t bit9, uint8_t queueBit) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr) return;
    if (mode == n->mType) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
        CMapCell* m = Neighbour(n, side1);
        if (m != nullptr && static_cast<int32_t>(m->mType) == 1 && n->GetNeighbouringCell(dir1)->IsQueue() == 0) n->ModifyNeighbours(add1);
        m = Neighbour(n, side2);
        if (m != nullptr && static_cast<int32_t>(m->mType) == 1 && n->GetNeighbouringCell(dir2)->IsQueue() == 0) n->ModifyNeighbours(add2);
        if (flag) n->SetHFieldTile();
    }
    if (static_cast<int32_t>(n->mType) == 10 && (n->GetDirection() & bit10) != 0) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
        if (flag) n->SetHFieldTile();
    }
    if (static_cast<int32_t>(n->mType) == 9) {
        if ((n->GetDirection() & bit9) != 0) {
            self->ModifyNeighbours(mine);
            n->ModifyNeighbours(theirs);
            if (flag) n->SetHFieldTile();
        }
    } else if (n->IsQueue() != 0 && (n->GetNeighbours() & queueBit) != 0 && flag) {
        n->SetHFieldTile();
        QueueSound(self->mCellId);
    }
}

// One of the four diagonal neighbours: joined when it has the mode and both straight cells beside it are paths (mode 1). The two straight
// cells are read without a NULL check (the original's).
void Diagonal(CMapCell* self, CELL_MODES mode, bool flag, const MapDelta& delta, const MapDelta& check1, const MapDelta& check2, uint8_t mine,
              uint8_t theirs) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr || mode != n->mType) return;
    if (static_cast<int32_t>(Neighbour(self, check1)->mType) != 1) return;
    if (static_cast<int32_t>(Neighbour(self, check2)->mType) != 1) return;
    self->ModifyNeighbours(mine);
    n->ModifyNeighbours(theirs);
    if (flag) n->SetHFieldTile();
}

// Path B's first part: the straight neighbour joins when it is the current object's entry cell and faces this cell.
void JoinEntry(CMapCell* self, const MapDelta& delta, uint8_t direction, const CMapCell* entry, uint8_t mine, uint8_t theirs) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr) return;
    if (n->GetDirection() != direction) return;
    if (n != entry) return;
    self->ModifyNeighbours(mine);
    n->ModifyNeighbours(theirs);
}

// Path B's second part: the straight neighbour of the mode joins when it has fewer than two neighbours and its parent is the current
// cell (gLLCCDB68) or the side's halfword; a mode 1 cell then loses the two diagonals beside it.
void JoinEnd(CMapCell* self, CELL_MODES mode, const MapDelta& delta, uint16_t other, uint8_t mine, uint8_t theirs, uint8_t remove1,
             uint8_t remove2) {
    CMapCell* const n = Neighbour(self, delta);
    if (n == nullptr || mode != n->mType) return;
    if (n->NumberOfNeighbours() < 2 && (n->mParentID == gLLCCDB68 || n->mParentID == other)) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
    }
    if (static_cast<int32_t>(self->mType) == 1) {
        self->RemoveNeighbour(remove1);
        self->RemoveNeighbour(remove2);
    }
}

}  // namespace

// 0x10099998. Mode 1 (paths) links the cell with its eight neighbours of the same mode (a mode 3 cell becomes 1 and is time stamped first),
// then drops the diagonals beside straight queue neighbours. Modes 0 and 3 join the current object's entry cell and the straight neighbour
// a / b (or the kept pair) points to. Every mode keeps a and b for the next call.
void CMapCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b, bool flag) {
    const int32_t m = static_cast<int32_t>(mode);
    if (m == 1) {
        if (static_cast<int32_t>(mType) == 3) {
            mType = static_cast<CELL_MODES>(1);
            CMap* const map = gGameWorld->GetMap();
            const uint32_t stamp = map->MakeNewTimeStamp();
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
            id.mIndex = mCellId;
            gGameWorld->GetMap()->SetTimeStamp(stamp, id);
        }
        Straight(this, mode, flag, gMapDeltaMinusY, 1, 0x10, gMapDeltaPlusXPlusY, 4, 8, gMapDeltaMinusXPlusY, 0x40, 0x20, 0x10, 1, 0x10);
        Diagonal(this, mode, flag, gMapDeltaPlusXMinusY, gMapDeltaMinusY, gMapDeltaPlusX, 2, 0x20);
        Straight(this, mode, flag, gMapDeltaPlusX, 4, 0x40, gMapDeltaMinusXMinusY, 1, 0x80, gMapDeltaMinusXPlusY, 0x10, 0x20, 0x40, 4, 0x40);
        Diagonal(this, mode, flag, gMapDeltaPlusXPlusY, gMapDeltaPlusY, gMapDeltaPlusX, 8, 0x80);
        Straight(this, mode, flag, gMapDeltaPlusY, 0x10, 1, gMapDeltaPlusXMinusY, 4, 2, gMapDeltaMinusXMinusY, 0x40, 0x80, 1, 0x10, 1);
        Diagonal(this, mode, flag, gMapDeltaMinusXPlusY, gMapDeltaPlusY, gMapDeltaMinusX, 0x20, 2);
        Straight(this, mode, flag, gMapDeltaMinusX, 0x40, 4, gMapDeltaPlusXMinusY, 1, 2, gMapDeltaPlusXPlusY, 0x10, 8, 4, 0x40, 4);
        Diagonal(this, mode, flag, gMapDeltaMinusXMinusY, gMapDeltaMinusY, gMapDeltaMinusX, 0x80, 8);

        uint8_t dir = 1;
        do {
            if ((dir & GetNeighbours()) != 0) {
                CMapCell* const cell = GetNeighbouringCell(dir);
                if (cell != nullptr && cell->IsQueue() != 0) {
                    switch (dir) {
                    case 1:
                        RemoveNeighbour(2);
                        RemoveNeighbour(0x80);
                        break;
                    case 4:
                        RemoveNeighbour(2);
                        RemoveNeighbour(8);
                        break;
                    case 0x10:
                        RemoveNeighbour(8);
                        RemoveNeighbour(0x20);
                        break;
                    case 0x40:
                        RemoveNeighbour(0x80);
                        RemoveNeighbour(0x20);
                        break;
                    default:
                        break;
                    }
                }
            }
            dir = ADD_DIRECTION(dir, 4);
        } while (dir != 1);
    } else if (m == 0 || m == 3) {
        // The current object's entry cell (the Mac reads the object through the 32-bit word gLLCCDB6C; the host keeps the pointer whole).
        const uint16_t entryId = gLLCMCurrentObject->mF32;
        const CMapCell* const entry = reinterpret_cast<const CMapCell*>(reinterpret_cast<const char*>(gGameWorld) + 0x2d8 +
                                                                        (static_cast<int32_t>(entryId) - 1) * 0x44);
        int32_t x;
        int32_t y;
        if (gCalcNeighboursJoinEntry != 0) {
            JoinEntry(this, gMapDeltaMinusY, 0x10, entry, 1, 0x10);
            JoinEntry(this, gMapDeltaPlusX, 0x40, entry, 4, 0x40);
            JoinEntry(this, gMapDeltaPlusY, 1, entry, 0x10, 1);
            JoinEntry(this, gMapDeltaMinusX, 4, entry, 0x40, 4);
            if (gCalcNeighboursPrevA == 0x80 || gCalcNeighboursPrevB == 0x80) {
                x = 0;
                y = 0;
            } else {
                x = gCalcNeighboursPrevA;
                y = gCalcNeighboursPrevB;
            }
        } else {
            x = a;
            y = b;
        }
        if (y == 1) JoinEnd(this, mode, gMapDeltaMinusY, gCalcNeighboursIdMinusY, 1, 0x10, 2, 0x80);
        if (x == -1) JoinEnd(this, mode, gMapDeltaPlusX, gCalcNeighboursIdPlusX, 4, 0x40, 2, 8);
        if (y == -1) JoinEnd(this, mode, gMapDeltaPlusY, gCalcNeighboursIdPlusY, 0x10, 1, 8, 0x20);
        if (x == 1) JoinEnd(this, mode, gMapDeltaMinusX, gCalcNeighboursIdMinusX, 0x40, 4, 0x80, 0x20);
    }
    gCalcNeighboursPrevA = a;
    gCalcNeighboursPrevB = b;
}
