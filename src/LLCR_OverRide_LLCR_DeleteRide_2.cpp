#include <cstdint>
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "World.h"
#include "ActionRecorder.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "TrackCell.h"

// 0x1029e04e (name invented): the thing id LLCR_OverRide answers when no ride stands on the cell (a halfword, 0 at start).
uint16_t gLLCRNoRide = 0;

namespace {

// The ride's shape is a loaded file image (32-bit layout): width +0, height +4, then rows of 0xa0 bytes holding eight-byte entries whose
// first word is nonzero for a cell the ride covers (+8 + row * 0xa0 + column * 8).
int32_t ShapeWord(const uint8_t* shape, int32_t offset) { return *reinterpret_cast<const int32_t*>(shape + offset); }

CMapCell* CellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 + static_cast<int32_t>(id - 1) * 0x44);
}

// mulhw by 0x51eb851f, srawi 5, plus the sign bit: the original's division by 100.
int32_t Percent(int32_t product) {
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * static_cast<int64_t>(product)) >> 32);
    int32_t shifted = high >> 5;
    return shifted + static_cast<int32_t>(static_cast<uint32_t>(shifted) >> 31);
}

// A queue entry or exit cell loses the lock flag when fewer than two of the cells around it are entries or exits and its attribute bit 8 is clear.
void ReleaseLock(CMapCell* cell, uint8_t direction) {
    if (cell->mType == 1 && cell->IsFlag(0x20)) {
        int32_t count = 0;
        uint8_t turn = direction;
        do {
            uint8_t neighbours = cell->GetNeighbours();
            if ((turn & neighbours) != 0) {
                CMapCell* other = cell->GetNeighbouringCell(turn);
                if (other != nullptr && (other->IsEntrance() != 0 || other->IsExit() != 0)) count++;
            }
            turn = ADD_DIRECTION(turn, 4);
        } while (turn != direction);
        if (count <= 1 && (cell->mStatusFlags & 8) == 0) cell->UnSetFlag(0x20);
    }
}

// The cell of the shape's (column, row) for the ride at (x, y) turned by rotation; false when the cell lies outside the level.
bool ShapeCell(int32_t rotation, int32_t ox, int32_t oy, int32_t column, int32_t row, int32_t* cx, int32_t* cy) {
    if (rotation == 0xb4) {
        *cx = ox - column;
        *cy = oy - row;
    } else if (rotation == 0x5a) {
        *cx = ox + row;
        *cy = oy - column;
    } else if (rotation == 0x10e) {
        *cx = ox - row;
        *cy = oy + column;
    } else {
        *cx = ox + column;
        *cy = oy + row;
    }
    return CMap::IsXYValid(*cx, *cy) != 0;
}

}  // namespace

void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag) {
    int32_t rides = 0;
    int32_t mode = LLCC_SystemGetLastMode();
    int32_t ox = x;
    int32_t oy = y;
    uint16_t found;
    LLCR_OverRide(&found, &ox, &oy);
    if (found == gLLCRNoRide) return;

    uint16_t id = found;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    CObject* obj = static_cast<CObject*>(gThingArray[id].mThing);
    if (obj == nullptr) return;

    int32_t percent = obj->GetResidualValuePercentage();
    int32_t rotation = obj->mRotation;
    int32_t cost = 0;
    RideInfo* info = obj->GetDBInfo();
    const uint8_t* shape = static_cast<const uint8_t*>(info->Value(kRideSlot_Shape).mShape);
    if (info->Value(kRideSlot_HasQueue).mInteger != 0) {
        LLCC_SystemConstructQueueClickStack(obj);
        LLCC_SystemSetMode(3, false);
        gLLCC47E4 = 1;
        if (static_cast<int32_t>(obj->mF3C) > 0) {
            while (LLCC_SystemBacktrack(obj, true) != 0) {
            }
            int32_t queueCost = LLCC_SystemGetQueueCost();
            int32_t amount = Percent(percent * queueCost);
            gGameWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(amount));
        } else {
            LLCC_SystemResetClickStack();
        }
        gLLCC47E4 = 0;
        int32_t trackType = info->Value(kRideSlot_WhichTrackType).mInteger;
        if (trackType != 0) {
            if (trackType == 1) {
                LLCC_SystemConstructTrackClickStack(obj);
                LLCC_SystemSetModeNR(0xb, false);
                gLLCC47E0 = 1;
                while (LLCC_SystemBacktrack(obj, false) != 0) {
                }
                gLLCC47E0 = 0;
                LLCC_SystemConstructReverseTrackClickStack(obj);
                LLCC_SystemSetModeNR(0xb, false);
                gLLCC47E0 = 1;
                while (LLCC_SystemBacktrack(obj, false) != 0) {
                }
                gLLCC47E0 = 0;
                cost = static_cast<int32_t>(LLCC_SystemGetKartTrackCost() << 1);
            } else if (trackType > 0 && trackType < 3) {
                LLCC_SystemConstructTrackClickStack(obj);
                LLCC_SystemSetModeNR(0x10, false);
                gLLCC47E0 = 1;
                while (LLCC_SystemBacktrack(obj, false) != 0) {
                }
                gLLCC47E0 = 0;
                LLCC_SystemConstructReverseTrackClickStack(obj);
                LLCC_SystemSetModeNR(0x10, false);
                gLLCC47E0 = 1;
                while (LLCC_SystemBacktrack(obj, false) != 0) {
                }
                gLLCC47E0 = 0;
                cost = static_cast<int32_t>(LLCC_SystemGetWaterTrackCost() << 1);
            }
            CBankAccount* bank = gGameWorld->GetBankAccount();
            bank->MakeWithdrawal(static_cast<uint32_t>(Percent(cost * percent)));
        }
    }
    if (flag == 0) LLCC_SystemSetMode(mode, false);

    const int32_t width = ShapeWord(shape, 0);
    const int32_t height = ShapeWord(shape, 4);
    // The loop counters outlive the loops: the sound reads them (r31 and r29 of the original), and x is the argument itself when the shape has no rows.
    int32_t column = x;
    int32_t row;
    for (row = 0; row < height; row++) {
        for (column = 0; column < width; column++) {
            if (ShapeWord(shape, 8 + row * 0xa0 + column * 8) == 0) continue;
            int32_t cx, cy;
            if (!ShapeCell(rotation, ox, oy, column, row, &cx, &cy)) continue;
            uint16_t cellId = static_cast<uint16_t>(cx + (cy << 7) + 1);
            CMapCell* cell = CellOfId(cellId);
            if (cell == nullptr) continue;
            if (cell->IsRide() != 0) rides++;
            if (cell->IsEntrance() != 0) {
                uint8_t direction = cell->GetDirection();
                CMapCell* beyond = cell->GetNeighbouringCell(OPPOSITE(direction));
                ReleaseLock(beyond, direction);
            }
            if (cell->IsExit() != 0) {
                uint8_t direction = cell->GetDirection();
                CMapCell* beyond = cell->GetNeighbouringCell(direction);
                ReleaseLock(beyond, direction);
            }
        }
    }
    for (row = 0; row < height; row++) {
        for (column = 0; column < width; column++) {
            if (ShapeWord(shape, 8 + row * 0xa0 + column * 8) == 0) continue;
            int32_t cx, cy;
            if (!ShapeCell(rotation, ox, oy, column, row, &cx, &cy)) continue;
            uint16_t cellId = static_cast<uint16_t>(cx + (cy << 7) + 1);
            CMapCell* cell = CellOfId(cellId);
            if (cell == nullptr) continue;
            if (cell->IsFlag(0x20) != 0) cell->UnSetFlag(0x20);
            cell->Delete(0, 0);
            CTrackCell* track = cell->GetTrackCellEquiv();
            if (flag == 0) {
                track->Delete(0, 0, true);
            } else {
                track->UnSetFlag(0x10);
            }
        }
    }
    if (flag == 0 && obj->mF24 != 0) BMP_DestroyRide(obj->mF24);
    obj->mBase.KillMe();

    if (gActionRecorder.mRecording == 0) {
        int32_t event;
        if (rides < 4) {
            event = 0x96;
        } else if (rides < 8) {
            event = 0x97;
        } else if (rides < 0x10) {
            event = 0x98;
        } else {
            event = 0x99;
        }
        float fx = static_cast<float>(column);
        float fy = static_cast<float>(row);
        int32_t sx = static_cast<int32_t>(10.0f * fx);
        int32_t sy = static_cast<int32_t>(10.0f * fy);
        SOUND_SendEvent(0, gSoundBanks[3], event, sx, 0, sy);
    }
}
