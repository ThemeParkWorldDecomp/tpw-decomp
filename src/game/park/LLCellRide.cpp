// LLCellRide.cpp: name proven by __sinit_LLCellRide_cpp in the main application.

#include "ActionRecorder.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "intrface.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// LLCR_OverRide 0x10089210 (140 bytes): answers the halfword of the world at +0x1da732 when (x, y) lies in the entrance A column range
// of the balance (x from EntranceAPosX - 1 to the slot after it, 0x584, plus 1) and y equals EntranceAPosY (+0x580); otherwise it asks the
// general routine at 0x1008905c with the addresses of x and y.

void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y);   // 0x1008905c (untranslated stand-in)

void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    int32_t lo = gMainBalance.mValues[(0x57c - 8) / 4].mInteger - 1;
    int32_t hi = gMainBalance.mValues[(0x584 - 8) / 4].mInteger + 1;
    int32_t row = gMainBalance.mValues[(0x580 - 8) / 4].mInteger;
    if (x < lo || x > hi || y != row) {
        LLCR_OverRide(out, &x, &y);
    } else {
        *out = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(gGameWorld) + 0x1da732);
    }
}

// Unit v5932: LLCR_OverRide (the pointer overload, 0x1008905c), LLCM_TrackCellBoundary, LLCR_SetRide, LLCR_AddRideToDB.

// The words of this unit (initialised data of the Mac, zero at start).
uint16_t gLLCCE04C = 0;
uint16_t gLLCCE092 = 0;
int32_t gLLCCE094 = 0;
const uint8_t* gLLCRRideShape = nullptr;
RideInfo* gLLCCE098 = nullptr;
uint16_t gLLCCE09C = 0;

namespace {
// A balance value of a ride record (the Mac reads the word at 0x00c + 4 * slot; the host record keeps 8-byte values behind a
// 64-bit table header, so the Mac's offsets name other bytes here). Unit v5L28.
inline int32_t RideValue(RideInfo* ride, RideBalanceSlot slot) { return ride->Value(slot).mInteger; }
}  // namespace

// 0x1008905c: x and y are updated in place; the answer is the thing id of the cell under (x, y), 0 when there is none.
void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y) {
    if (CMap::IsXYValid(*x, *y) == 0) {
        *out = 0;
        return;
    }
    uint16_t id = static_cast<uint16_t>(*x + static_cast<int32_t>(static_cast<uint32_t>(*y) << 7) + 1);
    char* mapBase = reinterpret_cast<char*>(gGameWorld) + 0x2d8;
    char* cell = mapBase + static_cast<int32_t>(id - 1) * 0x44;
    int32_t kind = *reinterpret_cast<int32_t*>(cell + 8);
    if (kind != 4 && kind != 9 && kind != 10) {
        *out = 0;
        return;
    }
    if (*reinterpret_cast<uint16_t*>(cell + 0x10) != id) {
        id = *reinterpret_cast<uint16_t*>(cell + 0x10);
        cell = mapBase + static_cast<int32_t>(id - 1) * 0x44;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CMapCell*>(cell)->GetObjectThingId();
    uint16_t thingId = thing.mIndex;
    if (thingId != gLLCCE04C) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr,
                       "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        id = *reinterpret_cast<uint16_t*>(static_cast<char*>(gThingArray[thingId].mThing) + 0x30);
    }
    *x = static_cast<int32_t>(id - 1) & 0x7f;
    *y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(id - 1)) >> 7);
    *out = thingId;
}

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

// LLCC_DrawKartBlock 0x10089cfc (1308 bytes): marks the four cells of a 2x2 kart track block.

int32_t LLCC_DrawKartBlock(int32_t x, int32_t y, int32_t /*unused*/, int32_t flagA, int32_t flagB, uint8_t direction) {
    if (!CMap::IsXYValid(x, y)) return 1;
    if (!CMap::IsXYValid(x, y + 1)) return 1;
    if (!CMap::IsXYValid(x + 1, y)) return 1;
    if (!CMap::IsXYValid(x + 1, y + 1)) return 1;

    CMapCell* base = reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8);
    auto at = [&](int32_t index) {
        uint16_t i = static_cast<uint16_t>(index);
        return base + (static_cast<int32_t>(i) - 1);
    };
    CMapCell* c0 = at(x + y * 128 + 1);
    CMapCell* c1 = at(x + y * 128 + 2);
    CMapCell* c2 = at(x + (y + 1) * 128 + 1);
    CMapCell* c3 = at(x + (y + 1) * 128 + 2);
    // order of the original's registers: r29 = c0, r28 = c2, r27 = c1, r30 = c3
    CMapCell* cells[4] = {c0, c2, c1, c3};

    int32_t track = LLCC_SystemGetRideTrack();
    if (track != 1 && track != 2) return 1;
    int32_t checked = track == 1 ? 0x20b : 0x210;
    int32_t plain = track == 1 ? 0x10b : 0x110;

    if (flagB != 0) {
        for (CMapCell* cell : cells) {
            if (cell) {
                uint32_t d = ADD_DIRECTION(direction, 0x10);
                if (cell->Modify(static_cast<CELL_MODES>(checked), 5, static_cast<int32_t>(d & 0xff)) == 0) return 0;
            }
        }
    } else if (flagA == 0) {
        for (CMapCell* cell : cells) {
            if (cell) {
                uint32_t d = ADD_DIRECTION(direction, 0x10);
                cell->Modify(static_cast<CELL_MODES>(plain), 5, static_cast<int32_t>(d & 0xff));
            }
        }
    }
    return 1;
}

// Unit v5635: LLCR_PlaceRide (0x1008a248, 6,572 bytes; LLCellRide.cpp on the Mac). Places the selected ride on the map at
// (x, y) with its rotation: marks its cells (build != 0), checks them (check != 0) or previews them (both 0), then the
// entrance and exit queue cells and, for a kart / water track ride, the two track blocks.

// The unit's words (BSS on the Mac; names invented).
LLCRPlacement gLLCRPlacement;   // 0x102de0a0
int32_t gLLCRPlaceRotation;     // 0x102de0d0
int32_t gLLCRPlaceY;            // 0x102de0d4
int32_t gLLCRPlaceX;            // 0x102de0d8

namespace {

// The map cell with the 1 based id x + y * 128 + 1 (kept to a halfword, as the Mac does): the grid is at world + 0x2d8.
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

CMapCell* MapCellOf(uint16_t id) {
    CMapCell* base = reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8);
    return base + (static_cast<int32_t>(id) - 1);
}

// The track grid follows the map grid: world + 0x2d8 + 0x110000, 0x28 bytes a cell.
CTrackCell* TrackCellOf(uint16_t id) {
    CTrackCell* base = reinterpret_cast<CTrackCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 + 0x110000);
    return base + (static_cast<int32_t>(id) - 1);
}

CELL_MODES Mode(int32_t m) { return static_cast<CELL_MODES>(m); }

// The current object of the cell system (gLLCCDB6C on the Mac; see gLLCMCurrentObject).
CObject* CurrentObject() {
    if (gLLCMCurrentObject != nullptr &&
        static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject))) == gLLCCDB6C)
        return gLLCMCurrentObject;
    return nullptr;
}

// Joins a queue cell to the path: the cell becomes a path (mode 1) and its path neighbours are recalculated.
void JoinQueueCell(CMapCell* cell) {
    if (static_cast<int32_t>(cell->mType) != 1) cell->Modify(Mode(0x87), 0, 0);
    cell->Modify(Mode(1), 0, 0);
    cell->CalculateNeighbours(Mode(1), 0, 0, true);
    cell->Modify(Mode(0x81), 0, 0);
    cell->Modify(Mode(0x85), 0, 0);
    cell->Modify(Mode(0x86), 0, 0);
    cell->SetFlag(0x20);
    uint8_t dir = 1;
    do {
        if ((cell->GetNeighbours() & dir) != 0) {
            CMapCell* next = cell->GetNeighbouringCell(dir);
            if (next != nullptr && static_cast<int32_t>(next->mType) == 1) {
                next->CalculateNeighbours(Mode(1), 0, 0, true);
                next->Modify(Mode(0x81), 0, 0);
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);
}

}  // namespace

int32_t LLCR_PlaceRide(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t build, int32_t check) {
    return static_cast<int32_t>(reinterpret_cast<uintptr_t>(LLCR_PlaceRideBlock(x, y, a, rotation, build, check)));
}

LLCRPlacement* LLCR_PlaceRideBlock(int32_t x, int32_t y, int32_t /*unused*/, int32_t rotation, int32_t build, int32_t check) {
    gLLCCFirstBlockX = -1;
    gLLCCFirstBlockY = -1;
    gLLCCE0CC = 0;
    gLLCRPlacement.mTopLeftX = -1;
    gLLCRPlacement.mTopLeftY = -1;
    gLLCRPlacement.mEntranceX = -1;
    gLLCRPlacement.mEntranceY = -1;
    gLLCRPlacement.mEntranceDirection = 0;

    uint16_t parentId = 0;
    uint8_t entranceDir = 0;      // 0x94(r1)
    int32_t exitX = 0;            // 0x90(r1)
    int32_t exitY = 0;            // 0x8c(r1)
    uint8_t exitDir = 0;          // 0x8a(r1)
    bool exitSeen = false;        // 0x89(r1)
    bool entranceSeen = false;    // 0x88(r1)
    int32_t entranceX = 0;        // r13
    int32_t entranceY = 0;        // r14
    bool failed = false;          // r16
    int32_t blocks = 0;           // r21: the cells of kind 4
    // r26: the rotation's direction bit, set by the second loop. With no cell in the footprint the Mac passes the caller's r26 on.
    uint8_t dirBase = 0;

    LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{gLLCCE0E0});
    int32_t width, height;
    LLCR_GetWidthHeight(&width, &height);

    const int32_t marginX = (5 - width) / 2;    // 0x9c(r1)
    const int32_t marginY = (5 - height) / 2;   // 0x98(r1)
    const int32_t zeroX = 0;                    // 0x84(r1)
    const int32_t zeroY = 0;                    // 0x80(r1)
    int32_t k7c, k78, k74, k70, k6c, k15;
    if (gLLCC47C4 == 0) {
        x -= width / 2;
        y -= height / 2;
        k7c = marginY - marginX;
        k78 = -1 - marginX - marginY;
        k74 = -1 - marginX - marginX;
        k70 = -1 - marginY - marginY;
        k6c = -1 - marginY - marginX;
        k15 = marginX - marginY;
    } else {
        k15 = 0;
        k7c = 0;
        k78 = -5;
        k74 = -5;
        k70 = -5;
        k6c = -1 - height;
    }
    gLLCRPlaceX = x - marginX;
    gLLCRPlaceY = y - marginY;
    gLLCRPlaceRotation = rotation;

    // The map cell of the footprint's (col, row) under the rotation.
    auto place = [&](int32_t col, int32_t row, int32_t& cx, int32_t& cy) {
        if (rotation == 0xb4) {
            cx = k74 + ((x + 5) - col);
            cy = k70 + ((y + 5) - row);
        } else if (rotation == 0x5a) {
            cx = x + (row + k7c);
            cy = k78 + ((y + 5) - col);
        } else if (rotation == 0x10e) {
            cy = y + (col + k15);
            cx = k6c + ((x + 5) - row);
        } else {
            cx = x + (col + zeroX);
            cy = y + (row + zeroY);
        }
    };

    if (build != 0) {
        for (int32_t row = 0; row < height; ++row) {
            for (int32_t col = 0; col < width; ++col) {
                int32_t cx, cy;
                place(col, row, cx, cy);
                if (!CMap::IsXYValid(cx, cy)) continue;
                CMapCell* cell = MapCellOf(CellId(cx, cy));
                int32_t kind = LLCR_GetRideCell(col, row);
                if (kind == 4) ++blocks;
                if (build != 0 && kind != 0) cell->Modify(Mode(0x87), 0, 0);
            }
        }
        int32_t tx, ty;
        if (rotation == 0xb4) {
            tx = x + k74 + 5;
            ty = y + k70 + 5;
        } else if (rotation == 0x5a) {
            ty = y + k78 + 5;
            tx = x + k7c;
        } else if (rotation == 0x10e) {
            ty = y + k15;
            tx = x + k6c + 5;
        } else {
            tx = x + zeroX;
            ty = y + zeroY;
        }
        gLLCRPlacement.mTopLeftX = tx;
        gLLCRPlacement.mTopLeftY = ty;
        LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{CellId(tx, ty)});
        CObject* ride = LLCR_AddRideToDB(rotation);
        if (gActionRecorder.mRecording == 0) {
            int32_t event = blocks < 4 ? 0x8e : blocks < 8 ? 0x8d : blocks < 0x10 ? 0x8f : 0x90;
            float fx = 10.0f * static_cast<float>(x);
            float fy = 10.0f * static_cast<float>(y);
            SOUND_SendEvent(0, gSoundBanks[3], event, ppc_fctiwz(fx), 0, ppc_fctiwz(fy));
        }
        // The ride's cell bytes (+4 x, +6 y), reached by Mac offset.
        const uint8_t* rb = reinterpret_cast<const uint8_t*>(ride);
        parentId = static_cast<uint16_t>(rb[4] + (static_cast<uint32_t>(rb[6]) << 7) + 1u);
    }

    if (gLLCCE0DC == 0) LLCM_SetCurrentObject(false);

    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            int32_t cx, cy;
            place(col, row, cx, cy);
            dirBase = rotation == 0xb4 ? 0x10 : rotation == 0x5a ? 0x40 : rotation == 0x10e ? 4 : 1;
            if (!CMap::IsXYValid(cx, cy)) continue;
            uint16_t id = CellId(cx, cy);
            CMapCell* cell = MapCellOf(id);
            CTrackCell* track = TrackCellOf(id);
            int32_t kind = LLCR_GetRideCell(col, row);
            uint8_t dir = LLCR_GetRideCellDirection(col, row);
            if (build != 0 && kind != 0) {
                int32_t mode = kind;
                if (mode == 0xb) mode = 4;
                if (mode == 0x10) mode = 4;
                if (mode == 0x17) mode = 4;
                cell->Modify(Mode(0x87), 0, 0);
                cell->Delete(0, 0);
                cell->Modify(Mode(mode), 0, 0);
                cell->Modify(Mode(0x81), 0, 0);
                cell->ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{parentId});
                track->DeleteMesh();
                track->UnSetFlag(0xf);
            }
            switch (static_cast<uint32_t>(kind)) {
            case 0:
                break;
            case 1:   // a path cell of the ride
                if (build != 0) {
                    cell->SetFlag(0x20);
                    cell->CalculateNeighbours(Mode(kind), 0, 0, true);
                    cell->Modify(Mode(0x81), 0, 0);
                    uint8_t d = 1;
                    do {
                        if ((cell->GetNeighbours() & d) != 0) {
                            CMapCell* next = cell->GetNeighbouringCell(d);
                            if (next != nullptr && static_cast<int32_t>(next->mType) == 1) {
                                next->CalculateNeighbours(Mode(1), 0, 0, true);
                                next->Modify(Mode(0x81), 0, 0);
                            }
                        }
                        d = ADD_DIRECTION(d, 4);
                    } while (d != 1);
                } else if (check != 0) {
                    if (cell->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                    failed = true;
                }
                break;
            case 10: {   // the exit
                exitSeen = true;
                exitDir = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                switch (exitDir) {
                case 1: exitX = cx; exitY = cy - 1; break;
                case 4: exitY = cy; exitX = cx + 1; break;
                case 0x10: exitX = cx; exitY = cy + 1; break;
                case 0x40: exitY = cy; exitX = cx - 1; break;
                default: break;
                }
                if (build != 0) {
                    cell->ModifyDirection(ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row)));
                    CMapCell* next = cell->GetNeighbouringCell(cell->GetDirection());
                    if (next == nullptr || static_cast<int32_t>(next->mType) != 1) break;
                    next->CalculateNeighbours(Mode(1), 0, 0, true);
                    next->Modify(Mode(0x81), 0, 0);
                } else if (check != 0) {
                    uint8_t d = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                    if (cell->Modify(Mode(0x204), 0, d) == 0) return nullptr;
                } else if (row == 0) {
                    if (cell->Modify(Mode(0x104), 6, dirBase) == 0) failed = true;
                } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                    failed = true;
                }
                break;
            }
            case 9: {   // the entrance
                entranceSeen = true;
                entranceDir = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                switch (entranceDir) {
                case 1: entranceX = cx; entranceY = cy + 1; break;
                case 4: entranceY = cy; entranceX = cx - 1; break;
                case 0x10: entranceX = cx; entranceY = cy - 1; break;
                case 0x40: entranceX = cx + 1; entranceY = cy; break;
                default: break;
                }
                if (build != 0) {
                    gLLCRPlacement.mEntranceX = cx;
                    gLLCRPlacement.mEntranceY = cy;
                    gLLCRPlacement.mEntranceDirection = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                    cell->ModifyDirection(gLLCRPlacement.mEntranceDirection);
                    CMapCell* next = cell->GetNeighbouringCell(OPPOSITE(cell->GetDirection()));
                    if (next == nullptr || static_cast<int32_t>(next->mType) != 1) break;
                    next->CalculateNeighbours(Mode(1), 0, 0, true);
                    next->Modify(Mode(0x81), 0, 0);
                } else if (check != 0) {
                    uint8_t d = ADD_DIRECTION(dirBase, LLCR_GetRideCellDirection(col, row));
                    if (cell->Modify(Mode(0x204), 3, d) == 0) return nullptr;
                } else if (row == 0) {
                    if (cell->Modify(Mode(0x104), 6, dirBase) == 0) failed = true;
                } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                    failed = true;
                }
                break;
            }
            case 11:   // a kart track block
            case 16: { // a water track block
                const int32_t checked = kind == 11 ? 0x20b : 0x210;
                const int32_t plain = kind == 11 ? 0x10b : 0x110;
                if (build == 0) {
                    if (check != 0) {
                        if (cell->Modify(Mode(checked), 0, 0) == 0) return nullptr;
                        if (cell->IsRide() != 0) return nullptr;
                    } else if (cell->Modify(Mode(plain), 0, 0) == 0) {
                        failed = true;
                    }
                } else {
                    cell->SetFlag(0x10);
                    track->SetFlag(0x10);
                }
                break;
            }
            case 23: {   // the cell beside the ride the direction points at
                if (dir != 0) {
                    if (check != 0) {
                        if (cell->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                    } else if (build == 0) {
                        cell->Modify(Mode(0x104), 0, ADD_DIRECTION(dirBase, 0x10));
                    }
                }
                CMapCell* next = cell->GetNeighbouringCell(ADD_DIRECTION(dirBase, dir));
                if (check != 0) {
                    if (next == nullptr) return nullptr;
                    if (next->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                } else if (build == 0) {
                    if (next != nullptr)
                        next->Modify(Mode(0x104), 5, ADD_DIRECTION(dirBase, 0x10));
                    else
                        gLLCCC214 = 1;
                } else {
                    // No NULL check here (the original's).
                    next->Modify(Mode(0x87), 0, 0);
                    next->Modify(Mode(0x15), 0, 0);
                    next->Modify(Mode(0x83), 0, 0);
                    next->Modify(Mode(0x84), 0, 0);
                    next->Modify(Mode(0x81), 0, 0);
                }
                break;
            }
            default:   // 2 to 8, 12 to 15, 17 to 22, and every kind above 23 (unsigned)
                if (build == 0) {
                    if (check != 0) {
                        if (cell->Modify(Mode(0x204), 0, 0) == 0) return nullptr;
                    } else if (row == 0) {
                        if (cell->Modify(Mode(0x104), 6, dirBase) == 0) failed = true;
                    } else if (cell->Modify(Mode(0x104), 0, 0) == 0) {
                        failed = true;
                    }
                }
                break;
            }
        }
    }

    if (gLLCCE0DC == 0) LLCM_SetCurrentObject(false);
    entranceDir = OPPOSITE(entranceDir);
    const uint8_t exitBack = OPPOSITE(exitDir);

    if (entranceSeen || exitSeen) {
        if (failed) gLLCCC214 = 1;
        if (!CMap::IsXYValid(entranceX, entranceY)) return nullptr;
        CMapCell* queue = MapCellOf(CellId(entranceX, entranceY));
        if (LLCC_SystemGetRideQueue() != 0) {
            if (check != 0) {
                if (queue->Modify(Mode(0x204), 3, entranceDir) == 0) return nullptr;
            } else if (build != 0) {
                CMapCell* door = MapCellOf(CellId(gLLCRPlacement.mEntranceX, gLLCRPlacement.mEntranceY));
                door->ModifyNeighbours(entranceDir);
                door->ModifyDirection(entranceDir);
                queue->Modify(Mode(0x87), 0, 0);
                queue->Modify(Mode(3), 0, 0);
                queue->ModifyNeighbours(OPPOSITE(entranceDir));
                queue->ModifyDirection(OPPOSITE(entranceDir));
                queue->ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{door->mParentID});
                queue->SetFlag(0x20);
                queue->Modify(Mode(0x81), 0, 0);
                queue->Modify(Mode(0x85), 0, 0);
                queue->Modify(Mode(0x86), 0, 0);
                CurrentObject()->RecalculateBackOfQueue();
            } else {
                queue->Modify(Mode(0x104), 3, entranceDir);
            }
        } else if (check != 0) {
            if (queue->Modify(Mode(0x201), 7, 0) == 0) return nullptr;
        } else if (build != 0) {
            JoinQueueCell(queue);
        } else {
            queue->Modify(Mode(0x118), 7, entranceDir);
        }

        if (LLCC_SystemGetRideQueue() != 0) {
            if (!CMap::IsXYValid(exitX, exitY)) return nullptr;
            CMapCell* out = MapCellOf(CellId(exitX, exitY));
            if (check != 0) {
                if (out->Modify(Mode(0x201), 0, exitBack) == 0) return nullptr;
            } else if (build != 0) {
                JoinQueueCell(out);
            } else {
                out->Modify(Mode(0x118), 4, exitBack);
            }
        }
    }

    int32_t trackKind = LLCC_SystemGetRideTrack();
    if (trackKind >= 3 || trackKind < 1) return &gLLCRPlacement;

    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{gLLCCE0E0});
    auto adjust = [&](int32_t slot) { return info->Value(static_cast<RideBalanceSlot>(slot)).mInteger; };
    int32_t startX, startY, endX, endY;
    if (rotation == 0xb4) {
        int32_t bx = adjust(kRideSlot_SouthXAdjust) + (((x + 4) - marginX) - marginX);
        endX = bx - 4;
        startX = bx - 9;
        int32_t by = ((y + 4) - marginY) - marginY;
        endY = adjust(kRideSlot_SouthYAdjust) + (by - 1);
        startY = adjust(kRideSlot_SouthYAdjust) + (by - 3);
    } else if (rotation == 0x5a) {
        int32_t bx = adjust(kRideSlot_WestXAdjust) + ((x + marginY) - marginX);
        endX = bx - 1;
        startX = bx;
        int32_t by = ((y + 4) - marginX) - marginY;
        endY = adjust(kRideSlot_WestYAdjust) + (by + 2);
        startY = adjust(kRideSlot_WestYAdjust) + (by - 3);
    } else if (rotation == 0x10e) {
        int32_t bx = adjust(kRideSlot_EastXAdjust) + (((x + 4) - marginY) - marginX);
        endX = bx - 1;
        startX = bx - 3;
        int32_t by = (y + marginX) - marginY;
        startY = adjust(kRideSlot_EastYAdjust) + by;
        endY = adjust(kRideSlot_EastYAdjust) + (by - 4);
    } else {
        int32_t bx = adjust(kRideSlot_NorthXAdjust) + x;
        endX = bx + 8;
        startX = bx;
        startY = adjust(kRideSlot_NorthYAdjust) + y;
        endY = adjust(kRideSlot_NorthYAdjust) + (y - 1);
    }
    if (static_cast<uint8_t>(LLCC_DrawKartBlock(startX, startY, rotation, build, check, dirBase)) == 0) return nullptr;
    LLCR_OffsetEndOfTrack(&endX, &endY, rotation);
    if (static_cast<uint8_t>(LLCC_DrawKartBlock(endX, endY, rotation, build, check, dirBase)) == 0) return nullptr;
    return &gLLCRPlacement;
}

// 0x1008bc20
void LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    gLLCCE09C = id.mIndex;
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> again{gLLCCE09C};
    gLLCCE098 = gRideDatabase.GetRideInfo(again);
    // The word 0x18 is the shape slot: a native pointer here, to a block the balance loader takes from the heap, so the host keeps
    // it whole beside the word. (Unit v5L28: the word at the Mac's offset 0x18 of the host record, read as an arena offset, sent
    // LLCR_GetWidthHeight and LLCR_GetRideCell to random addresses while a bought ride was placed.)
    gLLCRRideShape = static_cast<const uint8_t*>(gLLCCE098->Value(kRideSlot_Shape).mShape);
    gLLCCE094 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gLLCRRideShape));
}

namespace {
// gLLCCE094 is a 32-bit pointer on the Mac (the selected ride record's word 0x18, its shape); the host reads the shape through
// gLLCRRideShape, the whole pointer LLCR_SetRide keeps beside it (unit v5L28; it was read as an arena offset).
inline const uint8_t* RideCellAddress(int32_t col, int32_t row, uint32_t extra) {
    // (32-bit address arithmetic on the Mac: a negative row or column steps back from the shape)
    return gLLCRRideShape + static_cast<int32_t>(static_cast<uint32_t>(row) * 0xa0u + static_cast<uint32_t>(col) * 8u + extra);
}
}  // namespace

// 0x1008bccc
int32_t LLCR_GetRideCell(int32_t col, int32_t row) {
    return *reinterpret_cast<const int32_t*>(RideCellAddress(col, row, 8));
}

// 0x1008bd14
uint8_t LLCR_GetRideCellDirection(int32_t col, int32_t row) {
    return *RideCellAddress(col, row, 0xc);
}

// 0x1008bd68
void LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    gLLCCE092 = cell.mIndex;
}

// 0x1008bdcc
void LLCR_GetWidthHeight(int32_t* width, int32_t* height) {
    const uint8_t* record = gLLCRRideShape;
    *width = *reinterpret_cast<const int32_t*>(record);
    record = gLLCRRideShape;
    *height = *reinterpret_cast<const int32_t*>(record + 4);
}

// 0x1008be1c. A rotation other than 0, 0x5a, 0xb4 or 0x10e leaves the track kind (r28) unset on the Mac: it is taken as 0 here.
CObject* LLCR_AddRideToDB(int32_t rotation) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideId{gLLCCE09C};
    RideInfo* ride = gRideDatabase.GetRideInfo(rideId);
    int32_t built = 0;
    if (RideValue(ride, kRideSlot_BumperType) != 0) {
        uint16_t cellId = gLLCCE092;
        int32_t x = static_cast<int32_t>(cellId - 1) & 0x7f;
        int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(cellId - 1)) >> 7);
        int32_t kind = 0;
        if (rotation == 0xb4) {
            kind = 6;
            x += RideValue(ride, kRideSlot_SouthXAdjust);
            y += RideValue(ride, kRideSlot_SouthYAdjust);
        } else if (rotation == 0x5a) {
            kind = 5;
            x += RideValue(ride, kRideSlot_EastXAdjust);
            y += RideValue(ride, kRideSlot_EastYAdjust);
        } else if (rotation == 0) {
            kind = 0;
            x += RideValue(ride, kRideSlot_NorthXAdjust);
            y += RideValue(ride, kRideSlot_NorthYAdjust);
        } else if (rotation == 0x10e) {
            kind = 1;
            x += RideValue(ride, kRideSlot_WestXAdjust);
            y += RideValue(ride, kRideSlot_WestYAdjust);
        }
        LLCR_OffsetStartOfTrack(&x, &y, rotation);
        float fy = static_cast<float>(x);   // x goes to the second argument
        float fx = static_cast<float>(y);
        fy = 10.0f * fy;
        fx = 10.0f * fx;
        fy = 307.2f * fy;
        fx = 307.2f * fx;
        int32_t a = ppc_fctiwz(fy);
        int32_t b = ppc_fctiwz(fx);
        built = BMP_CreateRide(RideValue(ride, kRideSlot_BumperType), a, b, kind, 0, gLLCCE09C);
    }
    MapDelta delta = reinterpret_cast<MapDelta*>(&ride->mMapOffsetX)->Rotate(rotation);   // the record's 0x7ac
    uint16_t at = static_cast<uint16_t>(delta.mX + static_cast<int32_t>(static_cast<uint32_t>(delta.mY) << 7) + gLLCCE092);
    void* block = tpw::ArenaAlloc(sizeof(CObject));   // the Mac's game heap: the arena (unit v5L31; a list keeps a thing's address in 32 bits)
    CObject* object = block == nullptr ? nullptr
                                       : new (block) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{at},
                                                             tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{gLLCCE09C}, rotation, built);
    if (object != nullptr) {
        if (gLLCCE0DC != 0 && RideValue(ride, kRideSlot_UpgradeType) != 2) {
            CObject* current = LLCM_GetCurrentObject();
            object->SetUpgradeParent(reinterpret_cast<CThing*>(current)->GetThingId());
        } else {
            object->SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0});
        }
        gUIInterface.SetSelectedObject(object);
    }
    return object;
}

// The placement record (LLCR_PRR) is only read by Mac offset: +0 x, +4 y, +8 track x, +0xc track y, +0x10 direction (byte).
static int32_t PrrWord(LLCR_PRR* p, int32_t offset) { return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(p) + offset); }
static uint8_t PrrDirection(LLCR_PRR* p) { return *(reinterpret_cast<uint8_t*>(p) + 0x10); }

static uint8_t* MapCellAt(uint16_t id) {
    return reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44;
}

static uint8_t* TrackCellAt(uint16_t id) {
    return reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000;
}

// 0x1008c128.
CCellBase* LLCR_StartQueue(LLCR_PRR* placement, int32_t* x, int32_t* y) {
    if (!CMap::IsXYValid(PrrWord(placement, 0), PrrWord(placement, 4))) return nullptr;
    uint16_t id = static_cast<uint16_t>(PrrWord(placement, 0) + PrrWord(placement, 4) * 128 + 1);
    CCellBase* first = reinterpret_cast<CCellBase*>(MapCellAt(id));
    first->ModifyNeighbours(OPPOSITE(PrrDirection(placement)));
    *x = PrrWord(placement, 0);
    *y = PrrWord(placement, 4);
    switch (PrrDirection(placement)) {
        case 0x10: *y = *y - 1; break;
        case 4: *x = *x - 1; break;
        case 1: *y = *y + 1; break;
        case 0x40: *x = *x + 1; break;
        default: break;
    }
    if (!CMap::IsXYValid(*x, *y)) return nullptr;
    uint16_t id2 = static_cast<uint16_t>(*x + *y * 128 + 1);
    CMapCell* cell = reinterpret_cast<CMapCell*>(MapCellAt(id2));
    if (cell->ValidTypeChange(static_cast<CELL_MODES>(3))) cell->ModifyDirection(PrrDirection(placement));
    return cell;
}

// 0x1008c320. A track kind other than 1 or 2 leaves the first mode argument as the placement's address (low 32 bits). A rotation other
// than 0, 0x5a, 0xb4 or 0x10e leaves the coordinates unset on the Mac (callee-saved garbage): they are taken as 0 here.
CTrackCell* LLCR_StartTrack(LLCR_PRR* placement, int32_t* x, int32_t* y, int32_t track) {
    int32_t px = PrrWord(placement, 8), py = PrrWord(placement, 0xc);
    CELL_MODES mode = static_cast<CELL_MODES>(static_cast<int32_t>(reinterpret_cast<uintptr_t>(placement)));
    if (track == 2) mode = static_cast<CELL_MODES>(0x12);
    else if (track >= 1 && track < 2) mode = static_cast<CELL_MODES>(0xd);
    int32_t r30 = px, r29 = py;
    int32_t r25 = 0, r26 = 0, r27 = 0, r28 = 0;
    int32_t endX = 0, endY = 0;
    switch (gLLCRPlaceRotation) {
        case 0:
            r30 += 4; r29 += 1;
            endX = r30; r27 = r29; r25 = r29;
            r26 = r30 - 4; r28 = r30 - 4;
            endY = r27;
            endX -= 2;
            gLLCCE0CC = 4;
            break;
        case 0x5a:
            r29 -= 5; r30 += 1;
            endY = r29; r28 = r30; r26 = r30;
            r25 = r29 + 5; endX = r28; r27 = r29 + 4;
            endY += 2;
            gLLCCE0CC = 1;
            break;
        case 0xb4:
            r30 -= 5; r29 -= 2;
            endX = r30; r27 = r29; r25 = r29;
            r26 = r30 + 5; endY = r27; r28 = r30 + 4;
            endX += 2;
            gLLCCE0CC = 0x40;
            break;
        case 0x10e:
            r29 += 4; r30 -= 2;
            endY = r29; r28 = r30; r26 = r30;
            r25 = r29 - 4; endX = r28; r27 = r29 - 4;
            endY -= 2;
            gLLCCE0CC = 0x10;
            break;
        default: break;
    }
    LLCC_DrawTrackLine(mode, r28, r27, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x80), r28, r27, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x82), r28, r27, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x83), r28, r27, &endX, &endY);
    if (CMap::IsXYValid(r28, r27)) {
        gLLCCFirstBlockX = r28;
        gLLCCFirstBlockY = r27;
    }
    if (!CMap::IsXYValid(r30, r29)) return nullptr;
    *x = r30;
    *y = r29;
    uint16_t startId = static_cast<uint16_t>(r26 + r25 * 128 + 1);
    uint16_t endId = static_cast<uint16_t>(*x + *y * 128 + 1);
    gLLCCDB44 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(TrackCellAt(startId)));
    return reinterpret_cast<CTrackCell*>(TrackCellAt(endId));
}

// The track end offsets of the ride tools (main app, LLCellRide.cpp range 0x1008c620 to 0x1008c6d8, SimTheme Park 2.04b3; unit w204):
// LLCR_OffsetStartOfTrack 0x1008c620 and LLCR_OffsetEndOfTrack 0x1008c6d8. Each moves a cell (two words the caller passes by address, read
// first and stored x then y at the end) by an amount that depends on the ride's rotation (0, 90, 180 or 270 degrees; a signed compare) and answers a
// compass bit. For any other rotation the cells stay and the Mac answers a register it never set (the caller's r0 in the first, the low byte of
// the link register in the second): the host answers 0 there, and the test compares only the cells.

// 0x1008c620, 132 bytes: the cell of the first track piece and the direction bit it points to.
uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    int32_t newX = *x;
    int32_t newY = *y;
    uint8_t direction = 0;
    switch (rotation) {
        case 0:
            direction = 4;
            newX += 4;
            newY += 1;
            break;
        case 0x5a:
            direction = 1;
            newX += 1;
            newY -= 5;
            break;
        case 0xb4:
            direction = 0x40;
            newX -= 5;
            newY -= 2;
            break;
        case 0x10e:
            direction = 0x10;
            newX -= 2;
            newY += 4;
            break;
        default:
            break;
    }
    *x = newX;
    *y = newY;
    return direction;
}

// 0x1008c6d8, 168 bytes: the cell of the last track piece; the answer is the OPPOSITE of the direction bit the first function would give for the
// same rotation (the bit of the end, which points back along the track).
uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    int32_t newX = *x;
    int32_t newY = *y;
    uint8_t direction = 0;
    switch (rotation) {
        case 0:
            direction = 4;
            newX -= 2;
            newY += 1;
            break;
        case 0x5a:
            direction = 1;
            newX += 1;
            newY += 1;
            break;
        case 0xb4:
            direction = 0x40;
            newX += 1;
            newY -= 2;
            break;
        case 0x10e:
            direction = 0x10;
            newX -= 2;
            newY -= 2;
            break;
        default:
            break;
    }
    *x = newX;
    *y = newY;
    return OPPOSITE(direction);
}

// 0x1008c7b0 (the same machine code as LLCC_SetTrackAnimReverse, walking from the start of the track)
void LLCC_SetTrackAnim(CObject* object, bool flag) {
    const uint8_t* obj = reinterpret_cast<const uint8_t*>(object);
    uint32_t packed = static_cast<uint16_t>(obj[4] + (obj[6] << 7) + 1);
    int32_t x = static_cast<int32_t>((packed - 1) & 0x7f);
    int32_t y = static_cast<int32_t>((packed - 1) >> 7);
    int32_t rotation = *reinterpret_cast<const int32_t*>(obj + 0x10);
    uint8_t direction = LLCR_OffsetStartOfTrack(&x, &y, rotation);
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

// Unit v5982: LLCC_BuildTrackDB 0x1008ce04 (4,048 bytes). Walks the track from the first block (gLLCCFirstBlockX / Y, moved four cells
// by the rotation gLLCC9F20) cell by cell, two cells per step, in the direction byte gLLCCE0CC. For every track cell it places the
// ride's track sections (BMP_TrackSectionPlaceSection, only when `flag` is false), numbers the segments, turns the corners and, for
// the cells that carry the start flag 0x10, switches the "disabled" bit of the ride animation's nodes. Answers nonzero while the
// track is whole (the walk got back to its start without a break).

// Two words of the image that no other file names (initialised data).
uint16_t gLLCCE0B0 = 0;        // 0x1029e0b0
uint16_t gLLCCE0B2 = 0;        // 0x1029e0b2  (the image holds 0; unit v5L39, was 0xffff)

namespace {

const char kThingZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kFoundUpgrade[] = "Found upgrade... modify?\n";

// The track cell of a packed cell id (1 + x + (y << 7)): the id 0 lands one cell before the grid, as on the Mac.
CTrackCell* CellOf(uint32_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int32_t>(id - 1) * 0x28 + 0x110000);
}

// The scale the original applies to a cell coordinate: single precision, 10 times 307.2, truncated.
int32_t Scaled(int32_t cell) {
    float value = static_cast<float>(cell);
    float tenth = 10.0f * value;
    float scaled = 307.2f * tenth;
    return static_cast<int32_t>(scaled);
}

void Place(int32_t ride, int32_t x, int32_t y, int32_t kind, int32_t count, uint32_t thing, int32_t info) {
    BMP_TrackSectionPlaceSection(ride, Scaled(x), Scaled(y), kind, count, thing, info);
}

// The node (or mini node) of a ride animation set that a data node index names; its first word is the flag word.
uint32_t* NodeFlags(tag_sRideAnimSet* set, int32_t index) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t i = static_cast<uint32_t>(index) + header->mF46;
    if (i < header->mNodeCount) return reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(header->mNodes.get()) + i * 0xa0);
    return reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(header->mMiniNodes.get()) + (i - header->mNodeCount) * 0x58);
}

void SetNode(tag_sRideAnimSet* set, int32_t index) { *NodeFlags(set, index) |= 0x80000010u; }
void ClearNode(tag_sRideAnimSet* set, int32_t index) { *NodeFlags(set, index) &= 0xffffffefu; }

// Whether a neighbouring cell makes the flagged cell's end "closed": a track cell with a mesh, or a cell of type 0xd / 0x12.
bool Closed(CTrackCell* cell) {
    if (cell == nullptr) return false;
    if (cell->IsJustTrack() != 0 && cell->mMeshInstance != 0) return true;
    return cell->mType == 0xd || cell->mType == 0x12;
}

}  // namespace

int32_t LLCC_BuildTrackDB(bool flag) {
    int32_t segment = 0;
    int32_t count = 0;        // r23: the flagged cells met
    uint32_t thing = 0;       // r20
    int32_t info = 0;         // r13
    int32_t whole = 1;        // r14
    uint32_t mask = 0;        // r25
    int32_t rotation = gLLCC9F20;
    int32_t startY = gLLCCFirstBlockY;
    int32_t startX = gLLCCFirstBlockX;
    uint8_t direction = gLLCCE0CC;
    if (rotation == 0xb4) {
        startX = startX + 4;
    } else if (rotation < 0xb4) {
        if (rotation == 0x5a) {
            startY = startY + 4;
        } else if (rotation < 0x5a && rotation == 0) {
            startX = startX - 4;
        }
    } else if (rotation == 0x10e) {
        startY = startY - 4;
    }
    int32_t x = startX;
    int32_t y = startY;
    CTrackCell* cell = CellOf(static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1));
    if (cell->IsTrackX() != 0) cell = CellOf(cell->mParentID);
    if (cell->IsTrack() == 0) return 0;

    CCellBase* parent = CellOf(cell->mParentID);
    CMapCell* mapCell = parent->GetMapCellEquiv();
    uint16_t thingId = mapCell->GetObjectThingId().mIndex;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
    int32_t ride = static_cast<int32_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(gThingArray[thingId].mThing) + 0x24));
    if (!flag) BMP_FreeTrack(ride);

    do {
        cell = CellOf(static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1));
        cell->GetMapCellEquiv();
        if (cell->IsTrack() == 0) {
            direction = 0;
        } else {
            if (cell->IsCorner() != 0) {
                if (!flag) {
                    uint8_t corner = cell->IsCorner();
                    if (corner == 0x41) {
                        Place(ride, x, y, 6, 0, 0, 0);
                    } else if (corner < 0x41) {
                        if (corner == 0x14) {
                            Place(ride, x, y, 8, 0, 0, 0);
                        } else if (corner < 0x14 && corner == 5) {
                            Place(ride, x, y, 5, 0, 0, 0);
                        }
                    } else if (corner == 0x50) {
                        Place(ride, x, y, 7, 0, 0, 0);
                    }
                    count = 0;
                    thing = 0;
                }
            } else if (cell->IsCrossRoad() != 0) {
                if (!flag) {
                    Place(ride, x, y, 0xb, 0, 0, 0);
                    count = 0;
                    thing = 0;
                }
            } else if (cell->IsStraight() != 0) {
                int32_t high = segment < 2 ? 0x10000 : 0;
                if (cell->IsFlag(0x10) != 0) {
                    int32_t px = x;
                    int32_t py = y;
                    uint16_t found;
                    if (x < gMainBalance.mValues[(0x57c - 8) / 4].mInteger - 1 || gMainBalance.mValues[(0x584 - 8) / 4].mInteger + 1 < x ||
                        y != gMainBalance.mValues[(0x580 - 8) / 4].mInteger) {
                        LLCR_OverRide(&found, &px, &py);
                    } else {
                        found = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(gGameWorld) + 0x1da732);
                    }
                    if (found != gLLCCE0B0) {
                        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
                        CObject* object = static_cast<CObject*>(gThingArray[found].mThing);
                        if (object != nullptr) {
                            thing = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(object) + 0x1c);
                            info = object->GetDBInfo()->Value(kRideSlot_Id).mInteger;   // the Mac's +0x10, by name: the host's record is laid out wider
                        }
                    }
                } else {
                    count = 0;
                    thing = 0;
                }
                if (!flag) {
                    uint8_t straight = cell->IsStraight();
                    if (straight == 0x44) {
                        int32_t kind = high | (cell->IsFlag(0x10) != 0 ? 0xc : 9);
                        Place(ride, x, y, kind, count, thing, info);
                    } else if (straight < 0x44 && straight == 0x11) {
                        int32_t kind = high | (cell->IsFlag(0x10) != 0 ? 0xd : 0xa);
                        Place(ride, x, y, kind, count, thing, info);
                    }
                }
            }

            if (cell->IsFlag(0x10) != 0) {
                uint32_t id = cell->mCellId;
                count = count + 1;
                int32_t cx = static_cast<int32_t>((id - 1) & 0x7f);
                int32_t cy = static_cast<int32_t>((id - 1) >> 7);
                if (count == 0) mask = 0;
                CTRACE(6, kFoundUpgrade);
                uint16_t found;
                if (cx < gMainBalance.mValues[(0x57c - 8) / 4].mInteger - 1 || gMainBalance.mValues[(0x584 - 8) / 4].mInteger + 1 < cx ||
                    cy != gMainBalance.mValues[(0x580 - 8) / 4].mInteger) {
                    LLCR_OverRide(&found, &cx, &cy);
                } else {
                    found = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(gGameWorld) + 0x1da732);
                }
                if (found != gLLCCE0B2) {
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
                    if (gThingArray[found].mThing != nullptr) {
                        uint32_t ride2 = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(gThingArray[found].mThing) + 0x1c);
                        CTrackCell* next = cell->GetNeighbouringCell(cell->GetDirection(), 2);
                        if (Closed(next)) mask |= 0x10;
                        next = cell->GetNeighbouringCell(OPPOSITE(cell->GetDirection()), 2);
                        if (Closed(next)) mask |= 1;
                        if (count == 2) {
                            if (mask == 0x10) {
                                int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                if (a != -1) SetNode(gRideAnimSets[ride2], a);
                                if (b != -1) ClearNode(gRideAnimSets[ride2], b);
                            } else if (mask < 0x10) {
                                if (mask == 1) {
                                    int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                    int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                    if (a != -1) SetNode(gRideAnimSets[ride2], a);
                                    if (b != -1) ClearNode(gRideAnimSets[ride2], b);
                                } else if (mask == 0) {
                                    int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                    if (a != -1) ClearNode(gRideAnimSets[ride2], a);
                                    int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                    if (b != -1) ClearNode(gRideAnimSets[ride2], b);
                                }
                            } else if (mask < 0x12) {
                                int32_t a = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 1);
                                if (a != -1) SetNode(gRideAnimSets[ride2], a);
                                int32_t b = FindDataNodeIndexByDisableCode(gRideAnimSets[ride2], 1, 0x10);
                                if (b != -1) SetNode(gRideAnimSets[ride2], b);
                            }
                        }
                    }
                }
            } else if (cell->mMeshInstance == 0 && cell->IsJustTrack() != 0) {
                cell->SetHFieldTile();
            }

            if (cell->IsCorner() != 0) {
                uint8_t opposite = OPPOSITE(direction);
                if ((opposite & cell->GetNeighbours()) == 0) opposite = direction;
                direction = static_cast<uint8_t>(opposite ^ cell->GetNeighbours());
                if (direction != cell->GetDirection()) {
                    cell->ModifyDirection(direction);
                    cell->SetHFieldTile();
                }
            }
        }

        if (direction == 0 || cell->NumberOfNeighbours() < 2) whole = 0;
        if (cell->NumberOfNeighbours() == 3) {
            CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
            if (next->IsTrack() != 0) {
                CTrackCell* again = cell->GetNeighbouringCell(direction, 2);
                uint8_t neighbours = again->GetNeighbours();
                if ((direction & (neighbours == 0 ? 1 : 0)) != 0) whole = 0;
            }
            if (cell->GetNeighbouringCell(direction, 2)->mType == 0) whole = 0;
        }
        cell->SetSegmentNumber(static_cast<int16_t>(segment));
        segment = segment + 1;
        if (direction == 0x10) {
            y = y + 2;
        } else if (direction < 0x10) {
            if (direction == 4) {
                x = x + 2;
            } else if (direction < 4 && direction == 1) {
                y = y - 2;
            }
        } else if (direction == 0x40) {
            x = x - 2;
        }
    } while ((x != startX || y != startY) && whole != 0);
    return whole;
}

// LLCC_TraceTrack 0x1008ddfc (2,212 bytes): walks the track from one cell to another (two cells a step, along the direction byte), and with the
// flag set writes the directions, finds the first and last cell of the segment, deletes the cells of the cross-road stretch and breaks the
// track at the ends. Answers the number of cells walked, or -1 when a cell of the walk is no track.

namespace {

// The track cell of a cell id (id 0 lands one cell before the grid), as the original computes it: world + 0x2d8 + 0x110000 + (id - 1) * 0x28.
CTrackCell* CellOfId_2(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + 0x110000 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// A track X cell stands for its parent.
CTrackCell* Resolve(CTrackCell* cell) {
    if (cell->IsTrackX()) return CellOfId_2(cell->mParentID);
    return cell;
}

int32_t CellX(uint16_t id) { return static_cast<int32_t>(static_cast<uint32_t>(id) - 1u) & 0x7f; }
int32_t CellY(uint16_t id) { return static_cast<int32_t>((static_cast<uint32_t>(id) - 1u) >> 7); }

void Step(uint8_t dir, int32_t& x, int32_t& y) {
    switch (dir) {
        case 1: y -= 2; break;
        case 4: x += 2; break;
        case 0x10: y += 2; break;
        case 0x40: x -= 2; break;
        default: break;
    }
}

}  // namespace

// The callers ignore the answer; other units' test fakes define LLCC_TraceTrack as void, so the header says void.
void LLCC_TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) {
    LLCC_TraceTrackCount(from, to, flag);
}

int32_t LLCC_TraceTrackCount(CTrackCell* from, CTrackCell* to, int32_t flag) {
    CTrackCell* startCell = nullptr;
    CTrackCell* endCell = nullptr;
    int32_t count = 0;
    int32_t startCross = 0;   // 0x74
    int32_t endCross = 0;     // 0x70

    from = Resolve(from);
    to = Resolve(to);
    int16_t segFrom = from->GetSegmentNumber();
    int16_t segTo = to->GetSegmentNumber();

    if (flag == 0 || (static_cast<int32_t>(segTo) - static_cast<int32_t>(segFrom)) >= 0) {
        uint16_t id = from->mCellId;
        int32_t lineEndX = -1;   // 0x68
        int32_t lineEndY = -1;   // 0x64
        int32_t x = CellX(id);
        int32_t y = CellY(id);
        int32_t lineX = x;
        int32_t lineY = y;
        uint8_t dir = from->GetDirection();
        int32_t prevX = -1;
        int32_t prevY = -1;
        bool done = false;
        do {
            CTrackCell* cell = CellOfId_2(static_cast<uint16_t>(x + (y << 7) + 1));
            cell = Resolve(cell);
            if (!cell->IsTrack()) return -1;
            int16_t seg = cell->GetSegmentNumber();
            if (flag != 0) {
                if (startCell == nullptr && seg == segFrom) {
                    startCell = cell;
                    startCross = 0;
                }
                if (startCell == nullptr && cell->IsCrossRoad()) {
                    startCross = 1;
                    startCell = cell;
                }
                if (seg == segTo) {
                    endCell = cell;
                    endCross = 0;
                }
                if (endCell == nullptr && cell->IsCrossRoad()) {
                    endCross = 1;
                    endCell = cell;
                }
            }
            if (cell->IsTrack() || cell->mType == static_cast<CELL_MODES>(0x19)) {
                if (cell->IsCorner()) {
                    if (count != 0) {
                        uint8_t other = OPPOSITE(dir);
                        if ((other & cell->GetNeighbours()) == 0) other = dir;
                        dir = static_cast<uint8_t>(other ^ cell->GetNeighbours());
                    }
                    lineEndX = prevX;
                    lineEndY = prevY;
                }
            }
            if (cell == to) {
                lineEndX = x;
                done = true;
                lineEndY = y;
            }
            if (flag != 0) {
                if (cell->IsCrossRoad()) {
                    cell->ModifyDirection(static_cast<uint8_t>(dir | cell->GetDirection()));
                } else {
                    cell->ModifyDirection(dir);
                }
            }
            count++;
            if (lineX != -1 && lineEndX != -1) {
                LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x10f), lineX, lineY, &lineEndX, &lineEndY);
                lineEndX = -1;
                lineEndY = -1;
                lineX = x;
                lineY = y;
            }
            prevX = x;
            prevY = y;
            Step(dir, x, y);
        } while (!done);

        if (flag != 0 && startCell != nullptr && endCell != nullptr && startCell != endCell) {
            id = startCell->mCellId;
            x = CellX(id);
            y = CellY(id);
            if (startCell->IsCrossRoad()) {
                dir = from->GetDirection();
            } else {
                dir = startCell->GetDirection();
            }
            bool stop = false;
            do {
                CTrackCell* cell = CellOfId_2(static_cast<uint16_t>(x + (y << 7) + 1));
                cell = Resolve(cell);
                cell->GetSegmentNumber();
                if (cell->IsTrack() || cell->mType == static_cast<CELL_MODES>(0x19)) {
                    if (cell->NumberOfDirections() == 1) {
                        dir = cell->GetDirection();
                    } else {
                        cell->ModifyDirection(static_cast<uint8_t>(cell->GetDirection() & ~dir));
                    }
                } else {
                    stop = true;
                }
                if (cell == endCell) stop = true;
                if (cell->IsCrossRoad()) {
                    cell->Delete(-1, -1, true);
                } else if (cell->IsStraight()) {
                    cell->Delete(0, 0, true);
                } else if (cell->IsCorner()) {
                    cell->Delete(0, 0, true);
                } else if (cell->NumberOfNeighbours() == 1) {
                    cell->Delete(0, 0, true);
                }
                Step(dir, x, y);
            } while (!stop);
        }
    }

    if (flag != 0) {
        bool breakable = from->IsStraight() || from->IsCorner();
        if (gLLCCDB6C != 0) {
            // The current object: the Mac's 32-bit word, the host keeps the whole pointer beside it while the two agree.
            char* object = (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C))
                               ? reinterpret_cast<char*>(gLLCMCurrentObject)
                               : reinterpret_cast<char*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
            BMP_FreeTrack(*reinterpret_cast<int32_t*>(object + 0x24));
        }
        if (static_cast<int32_t>(segTo) - static_cast<int32_t>(segFrom) <= 1) {
            uint16_t id = from->mCellId;
            if (breakable) LLCC_BreakTrack(CellX(id), CellY(id), 0);
        }
        if (startCross != 0) {
            uint16_t id = from->mCellId;
            if (breakable) LLCC_BreakTrack(CellX(id), CellY(id), 0);
        }
        if (endCross != 0) {
            uint8_t dir = from->GetDirection();
            uint16_t id = to->mCellId;
            if (breakable) LLCC_BreakTrack(CellX(id), CellY(id), OPPOSITE(dir));
        }
        if (startCell == endCell && startCell != nullptr && breakable) startCell->Delete(0, 0, true);
    }
    return count;
}

// LLCellSystem (LLCellRide.cpp of the main app, unit v5082): LLCC_Kill_Them_KILL_THEM_ALL 0x1008ebc0 (1,212 bytes) and LLCC_BuildHoardingDB
// 0x1008e6e0 (1,196 bytes). The cells are reached through the world: map cells 0x44 bytes at gGameWorld + 0x2d8, track cells 0x28 bytes at
// gGameWorld + 0x2d8 + 0x110000, ids 1 + x + (y << 7) as a halfword.

int32_t gLLCCHoardMinX;   // 0x102de0c8
int32_t gLLCCHoardMinY;   // 0x102de0c4
int32_t gLLCCHoardMaxX;   // 0x102de0c0
int32_t gLLCCHoardMaxY;   // 0x102de0bc

namespace {

CMapCell* MapCellAt_2(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

// One scan along a line: the cell at (x, y) is the origin; `step` moves along the axis. Returns nothing: clears ok like the original.
// (Written out four times in the original; the loops differ only in the axis, the step and the bound.)
}  // namespace

// 0x1008e6e0 (1,196 bytes): walks the hoarding from (x, y) cell by cell (two cells a step) along the direction byte, fixing corners and
// crossroads and giving the neighbouring cells to the hoarding, until the walk has passed (right, bottom); records the extents and redraws the
// current hoarding image. Always 1.
int32_t LLCC_BuildHoardingDB(int32_t right, int32_t bottom, int32_t left, int32_t top, char mode, bool flag) {
    int32_t x = right;
    int32_t y = bottom;
    int32_t targetX = left;
    int32_t targetY = top;
    uint8_t dir = static_cast<uint8_t>(mode);
    int32_t done = 0;
    gLLCCHoardMinX = 0x400;
    gLLCCHoardMinY = 0x400;
    gLLCCHoardMaxX = 0;
    gLLCCHoardMaxY = 0;
    do {
        uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
        CTrackCell* cell = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + 0x110000 +
                                                          static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
        if (cell->mType == 0x19 || (gLLCC4818 != 0 && cell->NumberOfNeighbours() != 0)) {
            if (cell->IsCorner()) {
                uint8_t other = OPPOSITE(dir);
                if ((other & cell->GetNeighbours()) == 0) other = dir;
                dir = static_cast<uint8_t>(other ^ cell->GetNeighbours());
            }
            if (cell->IsCrossRoad()) {
                uint8_t found = 0;
                uint8_t probe = ADD_DIRECTION(dir, 0x40);
                do {
                    if (probe & cell->GetNeighbours()) found = probe;
                    probe = ADD_DIRECTION(probe, 4);
                } while (found == 0);
                if (dir == found) {
                    uint8_t opposite = OPPOSITE(dir);
                    cell->RemoveNeighbour(static_cast<uint8_t>(~(dir | opposite)));
                    cell->Modify(static_cast<CELL_MODES>(0x81), 0, 0);
                    cell->SetHFieldTile();
                } else {
                    cell->ModifyDirection(found);
                    cell->RemoveNeighbour(dir);
                    cell->RemoveNeighbour(OPPOSITE(found));
                    cell->Modify(static_cast<CELL_MODES>(0x81), 0, 0);
                    cell->SetHFieldTile();
                }
                dir = found;
            }
        } else {
            dir = 0;
        }
        if (x == targetX && y == targetY) done = 1;
        if (flag && dir != cell->GetDirection()) {
            cell->ModifyDirection(dir);
            cell->Modify(static_cast<CELL_MODES>(0x81), 0, 0);
        }
        const MapDelta* deltas[3] = {&gMapDeltaPlusY, &gMapDeltaPlusX, &gMapDeltaPlusXPlusY};
        for (const MapDelta* source : deltas) {
            MapDelta delta;
            delta.mX = source->mX;
            delta.mY = source->mY;
            delta.mFlag = source->mFlag;
            CTrackCell* neighbour = cell->GetNeighbouringCell(delta, 1);
            if (neighbour != nullptr) {
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> parent;
                parent.mIndex = cell->mCellId;
                neighbour->ModifyParentID(parent);
                neighbour->ModifyType(static_cast<CELL_MODES>(0xc));
            }
        }
        switch (dir) {
            case 1: y -= 2; break;
            case 4: x += 2; break;
            case 0x10: y += 2; break;
            case 0x40: x -= 2; break;
            default: break;
        }
        if (x < gLLCCHoardMinX) gLLCCHoardMinX = x;
        if (y < gLLCCHoardMinY) gLLCCHoardMinY = y;
        if (x > gLLCCHoardMaxX) gLLCCHoardMaxX = x;
        if (y > gLLCCHoardMaxY) gLLCCHoardMaxY = y;
    } while (done == 0);
    LLCC_CreateCurrentHoardingImage();
    return 1;
}
// 0x1008ebc0 (1,212 bytes): for every cell of the rectangle that is a hoarding-like cell (mode 4, 9 or 10) whose run of equal parent cells
// along +x, -x, +y and -y stays inside the rectangle (plus one) and has no cell flagged 0x10, the ride is deleted; then every path cell of
// the rectangle is deleted. Always 1.
int32_t LLCC_Kill_Them_KILL_THEM_ALL(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int32_t xMin = x1 < x2 ? x1 : x2;
    int32_t yMin = y1 < y2 ? y1 : y2;
    int32_t xMax = x2 < x1 ? x1 : x2;
    int32_t yMax = y2 < y1 ? y1 : y2;
    // (the original picks the max with `p3 < p1 ? p3 : p1` style compares on the stack copies: min(a,b) when a<b else b, max the mirror)
    xMin = (x1 < x2) ? x1 : x2;
    xMax = (x2 < x1) ? x1 : x2;
    yMin = (y1 < y2) ? y1 : y2;
    yMax = (y2 < y1) ? y1 : y2;

    for (int32_t y = yMin; y <= yMax; y++) {
        for (int32_t x = xMin; x <= xMax; x++) {
            CMapCell* origin = MapCellAt_2(x, y);
            int32_t mode = origin->mType;
            bool candidate;
            if (mode < 9)
                candidate = (mode == 4);
            else
                candidate = (mode < 0xb);
            if (!candidate) continue;
            bool ok = true;
            int32_t cx = x;
            int32_t cy = y;
            CMapCell* cell;
            // +x
            do {
                cell = MapCellAt_2(cx, y);
                if (cell->IsFlag(0x10)) ok = false;
                if (cx > xMax + 1) ok = false;
                int32_t m = cell->mType;
                cx++;
                if (!(m == 4 || m == 9 || m == 10)) break;
                if (cell->mParentID != origin->mParentID) break;
            } while (CMap::IsXYValid(cx, y));
            cx = x;
            if (ok) {
                // -x
                do {
                    cell = MapCellAt_2(cx, y);
                    if (cell->IsFlag(0x10)) ok = false;
                    if (cx < xMin - 1) ok = false;
                    int32_t m = cell->mType;
                    cx--;
                    if (!(m == 4 || m == 9 || m == 10)) break;
                    if (cell->mParentID != origin->mParentID) break;
                } while (CMap::IsXYValid(cx, y));
            }
            cy = y;
            if (ok) {
                // +y
                do {
                    cell = MapCellAt_2(x, cy);
                    if (cell->IsFlag(0x10)) ok = false;
                    if (cy > yMax + 1) ok = false;
                    int32_t m = cell->mType;
                    cy++;
                    if (!(m == 4 || m == 9 || m == 10)) break;
                    if (cell->mParentID != origin->mParentID) break;
                } while (CMap::IsXYValid(x, cy));
            }
            cy = y;
            if (ok) {
                // -y
                do {
                    cell = MapCellAt_2(x, cy);
                    if (cell->IsFlag(0x10)) ok = false;
                    if (cy < yMin - 1) ok = false;
                    int32_t m = cell->mType;
                    cy--;
                    if (!(m == 4 || m == 9 || m == 10)) break;
                    if (cell->mParentID != origin->mParentID) break;
                } while (CMap::IsXYValid(x, cy));
            }
            if (ok) LLCR_DeleteRide(x, y, 0);
        }
    }
    for (int32_t y = yMin; y <= yMax; y++) {
        for (int32_t x = xMin; x <= xMax; x++) {
            CMapCell* cell = MapCellAt_2(x, y);
            if (cell->IsPath()) cell->Delete(0, 0);
        }
    }
    return 1;
}

int32_t gLLCCDB7C[0x102];  // 0x102ddb7c (name invented)

namespace {
// The jump table at 0x1029e0bc: the value the table gets for each cell kind 0 to 0x17 (0: nothing).
const int32_t kProfileKind[24] = {0, 4, 0, 0, 1, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
}  // namespace

// 0x1008f0b4 (196 bytes): remembers the pylon's cell, takes the mesh instance word from the cell's track cell (clearing it there),
// deletes the map cell unless flag, and marks a brace drag pending.
void LLCC_StartPylonMove(int32_t x, int32_t z, int32_t flag) {
    gLLCCE0B4 = x;
    gLLCCE0B8 = z;
    uint16_t index = static_cast<uint16_t>(x + (z << 7) + 1);
    CMapCell* cell = reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int32_t>(static_cast<uint16_t>(index) - 1) * 0x44);
    CTrackCell* track = cell->GetTrackCellEquiv();
    gLLCCE08C = track->mMeshInstance;
    track->mMeshInstance = 0;
    if (flag == 0) cell->Delete(0, 0);
    gLLCCE090 = 1;
}

// LLCC_AbortPylonMove 0x1008f1a4, LLCC_IsRideMovable 0x1008f330 and LLCC_GetRideBlendInfo 0x1008f574 (unit v5186).

int32_t gLLCCE0B4 = -1;   // 0x1029e0b4
int32_t gLLCCE0B8 = -1;   // 0x1029e0b8
uint32_t gLLCCE08C;       // 0x102de08c
LLCCRideBlend gLLCCDF84;  // 0x102ddf84

// 0x1008f1a4 (292 bytes): while a pylon drag is pending, draws the three lines back over its cell, gives the track cell's mesh word
// (+4) the saved value, ends the drag and forgets the cell.
void LLCC_AbortPylonMove() {
    int32_t x = gLLCCE0B4;
    int32_t y = gLLCCE0B8;
    if (x == -1 || y == -1 || gLLCCE090 == 0) return;
    LLCC_DrawLine(static_cast<CELL_MODES>(0x15), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x83), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x81), x, y, &x, &y);
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1].mMeshInstance = gLLCCE08C;
    gLLCCE090 = 0;
    gLLCCE0B4 = -1;
    gLLCCE0B8 = -1;
}

// 0x1008f2f4, 16 bytes.
void LLCC_ResetPylonMove() {
    gLLCCE090 = 0;
}

namespace {
CTrackCell* TrackCellOf_2(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return &gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1];
}
}  // namespace

// 0x1008f330 (528 bytes): the ride's x (byte +4) and y (byte +6) give a cell id; for a ride whose track type is 1 or 2 both the cell at the
// start offset and the cell at the end offset must have exactly one neighbour.
int32_t LLCC_IsRideMovable(CObject* ride) {
    int32_t result = 1;
    int32_t type = ride->GetDBInfo()->Value(kRideSlot_WhichTrackType).mInteger;
    if (type == 0 || type == 3 || type >= 3 || type < 0) return result;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(ride);
    for (int32_t pass = 0; pass < 2; pass++) {
        uint16_t id = static_cast<uint16_t>(bytes[4] + (bytes[6] << 7) + 1);
        int32_t t = static_cast<int32_t>(id) - 1;
        int32_t x = t & 0x7f;
        int32_t y = static_cast<int32_t>(static_cast<uint32_t>(t) >> 7);
        if (pass == 0)
            LLCR_OffsetStartOfTrack(&x, &y, ride->mRotation);
        else
            LLCR_OffsetEndOfTrack(&x, &y, ride->mRotation);
        if (TrackCellOf_2(x, y)->NumberOfNeighbours() != 1) return 0;
    }
    return result;
}

namespace {
// The cell (x, z) of the shape: ints, width at 0, height at 4, then rows of 20 cells of 8 bytes (the type is the first word).
bool IsBlendCell(const int32_t* shape, int32_t x, int32_t z) {
    if (x < 0 || z < 0 || shape[0] < x || shape[1] < z) return false;
    int32_t type = shape[2 + z * 40 + x * 2];
    return type == 4 || type == 0x17 || static_cast<uint32_t>(type - 9) < 2;
}
}  // namespace

// 0x1008f574 (732 bytes): copies the shape's size into the table, clears 16 x 16 bytes, then flags (x * 16 + z) every cell of the shape
// that is a hoarding-like cell (4, 9, 10, 0x17) without all four neighbours of that kind. Answers the table.
void* LLCC_GetRideBlendInfo(void* shape) {
    const int32_t* s = static_cast<const int32_t*>(shape);
    LLCCRideBlend* table = &gLLCCDF84;
    table->mWidth = s[0];
    table->mHeight = s[1];
    for (int32_t z = 0; z < 16; z++)
        for (int32_t x = 0; x < 16; x++) table->mCells[x * 16 + z] = 0;
    for (int32_t z = 0; z < s[1]; z++) {
        for (int32_t x = 0; x < s[0]; x++) {
            if (!IsBlendCell(s, x, z)) continue;
            int32_t count = 0;
            if (IsBlendCell(s, x, z - 1)) count++;
            if (IsBlendCell(s, x - 1, z)) count++;
            if (IsBlendCell(s, x + 1, z)) count++;
            if (IsBlendCell(s, x, z + 1)) count++;
            if (count != 4) table->mCells[x * 16 + z] = 1;
        }
    }
    return table;
}

// 0x1008f880 (280 bytes): copies the shape's size into the table, clears 16 x 16 words, then stores a value (1 to 4) at x * 16 + z for
// every cell of the shape whose kind (a word at 8 + z * 0xa0 + x * 8) is in the jump table. Answers the table.
int32_t* LLCC_GetRideProfileInfo(void* ride) {
    // The Mac reads the word at 0x18 (value slot 3 of the ride's balance); the host's slot holds a native pointer.
    const int32_t* s = static_cast<const int32_t*>(static_cast<RideInfo*>(ride)->Value(static_cast<RideBalanceSlot>(3)).mShape);
    int32_t* table = gLLCCDB7C;
    table[0x100] = s[0];
    table[0x101] = s[1];
    for (int32_t z = 0; z < 16; z++)
        for (int32_t x = 0; x < 16; x++) table[x * 16 + z] = 0;
    for (int32_t z = 0; z < s[1]; z++) {
        for (int32_t x = 0; x < s[0]; x++) {
            uint32_t kind = static_cast<uint32_t>(s[2 + z * 40 + x * 2]);
            if (kind > 0x17) continue;
            int32_t v = kProfileKind[kind];
            if (v != 0) table[x * 16 + z] = v;
        }
    }
    return table;
}

// 0x1008f9c8, 52 bytes: the four limits of the hoarding walk.
void LLCR_GetHoardingExtents(int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    *a = gLLCCHoardMinX;
    *b = gLLCCHoardMinY;
    *c = gLLCCHoardMaxX;
    *d = gLLCCHoardMaxY;
}

namespace {

// The track cell of a (1 based) cell id; the Mac subtracts one in 32 bits, so id 0 lands one cell before the grid.
CTrackCell* TrackCellAt_2(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x1102d8 +
                                         static_cast<int32_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

void Step_2(uint8_t dir, int32_t& x, int32_t& y) {
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

    if (!TrackCellAt_2(x, y)->IsTrack()) {
        result = 1;
        found = 1;
    }

    do {
        CTrackCell* cell = TrackCellAt_2(x, y);
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
                    Step_2(dir, x, y);
                }
            }
        }
        if (!found) Step_2(dir, x, y);
    } while (!found);
    return static_cast<uint8_t>(result);
}

// LLCR_FixBridgesReverse 0x1009024c (2216 bytes): walks a ride's track from its end backwards, one cell (two steps) at a time, and
// rebuilds the bridge pieces' flags; stops when the track runs out or a cell does not continue in the walking direction.

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

// 0x10090b2c, 16 bytes.
void __sinit_LLCellRide_cpp() {
    gLLCCE092 = 0;
}
