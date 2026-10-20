// LLCM_LeftMouseButtonUp (main app 0x10084ca4, 14,520 bytes; unit v5883): the cell system's left button release. Each edit mode
// finishes its drag here: paths and queues (1, 3), ride placement (4, 0x3b), track (0xb, 0x10, 0xe, 0x13, 0x1a), hoardings (0x19,
// 0x39), land clearing (0x3a), ride deletion (0x33), the line modes (0x32, 0x34, 0x35), staff patrol areas (0x36), queue editing
// (0x14), coaster pylons (0x15, 0x16), and the illegal placement's tidy up. Every exit clears gLLCCDB34 (set by the button down).
//
// Notes from the listing:
// - The cells are found by address arithmetic on the world block: the map cell of index i at +0x2d8 + (i - 1) * 0x44, the track
//   cell at +0x2d8 + 0x110000 + (i - 1) * 0x28 (the index is 1 + x + (y << 7), a halfword).
// - LLCC_DrawLine, LLCC_DrawTrackLine and LLCC_TraceTrack are declared void, but the listing reads their r3 (the 0x81 line's answer
//   decides whether the drag ends, TraceTrack's is compared with -1): they are called through an int-answering pointer type.
// - The click modes re-read the current object word gLLCCDB6C each time; gLLCCDB64 is the selected peep.
// - gCalcNeighboursPrevA is stored twice where the other modes store PrevA and PrevB (the original's slip, kept).
// - The illegal path's case 0x100884f8 stores the original arguments back into their own slots; nothing reads them after.
#include <cstdint>
#include <cstdlib>

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

int32_t gLLCC9F28 = -1;
int32_t gLLCC9F2C = -1;
int32_t gLLCC9F44 = 1;
int32_t gLLCC9F48 = 1;
int32_t gLLCCDB30;
int32_t gLLCCDB44;
CCellBase* gLLCCDB4C;
CThing* gLLCCDB64;

namespace {

using LineFn = int32_t (*)(CELL_MODES, int32_t, int32_t, int32_t*, int32_t*);
using TraceFn = int32_t (*)(CTrackCell*, CTrackCell*, int32_t);

int32_t DrawLine(int32_t mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    return reinterpret_cast<LineFn>(&LLCC_DrawLine)(static_cast<CELL_MODES>(mode), x, y, endX, endY);
}

int32_t DrawTrackLine(int32_t mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    return reinterpret_cast<LineFn>(&LLCC_DrawTrackLine)(static_cast<CELL_MODES>(mode), x, y, endX, endY);
}

int32_t TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) {
    return reinterpret_cast<TraceFn>(&LLCC_TraceTrack)(from, to, flag);
}

uint8_t* WorldBytes() { return reinterpret_cast<uint8_t*>(gGameWorld); }

uint16_t CellIndex(int32_t x, int32_t y) { return static_cast<uint16_t>(x + (y << 7) + 1); }

CMapCell* MapCellAt(uint16_t index) {
    return reinterpret_cast<CMapCell*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44);
}

CTrackCell* TrackCellAt(uint16_t index) {
    return reinterpret_cast<CTrackCell*>(WorldBytes() + 0x2d8 + 0x110000 + (static_cast<int32_t>(index) - 1) * 0x28);
}

int32_t CellX(uint16_t index) { return static_cast<int32_t>((static_cast<uint32_t>(index) - 1) & 0x7f); }
int32_t CellY(uint16_t index) { return static_cast<int32_t>((static_cast<uint32_t>(index) - 1) >> 7); }

// The current object: the Mac's word gLLCCDB6C; the host keeps the whole pointer in gLLCMCurrentObject while the two agree.
CObject* Current() {
    if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C)) return gLLCMCurrentObject;
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}

void SetCurrent(CObject* object) {
    gLLCMCurrentObject = object;
    gLLCCDB6C = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(object)));
}

int32_t Recording() { return gActionRecorder.mRecording; }

// The click sound at the cell (x, y): the cell scaled by 10 in single precision, truncated (fctiwz).
void Sound(int32_t event, int32_t x, int32_t y) {
    const int32_t px = static_cast<int32_t>(10.0f * static_cast<float>(x));
    const int32_t py = static_cast<int32_t>(10.0f * static_cast<float>(y));
    SOUND_SendEvent(0, gSoundBanks[3], event, px, 0, py);
}

void Advise(int32_t message) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> nobody;
    nobody.mIndex = 0;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), nobody, 0, -1, false, -1);
    gGameWorld->GetAdvisor()->QueueRequest(request);
}

// The end of a drag: no line start, the neighbour state back to its start.
void EndDragWords() {
    gLLCC4804 = -1;
    gLLCC4800 = -1;
    gCalcNeighboursPrevA = 0x80;
    gCalcNeighboursPrevA = 0x80;
    gLLCCE288 = 0x80;
    gLLCCE28C = 0x80;
}

// Snaps (x, y) to the line from (gLLCC4804, gLLCC4800): the shorter offset goes.
void Snap(int32_t* x, int32_t* y) {
    int32_t a = std::abs(*y - gLLCC4800);
    int32_t b = std::abs(*x - gLLCC4804);
    if (!(b < a)) *y = gLLCC4800;
    a = std::abs(*y - gLLCC4800);
    b = std::abs(*x - gLLCC4804);
    if (!(b > a)) *x = gLLCC4804;
}

uint8_t DirectionOfRotation(int32_t rotation) {
    if (rotation == 0xb4) return 0x40;
    if (rotation == 0x5a) return 1;
    if (rotation == 0) return 4;
    if (rotation == 0x10e) return 0x10;
    return 0;
}

// The track switches of modes 0xe and 0x13 after the click stack is rebuilt.
void TrackStackAgain() {
    LLCC_SystemConstructTrackClickStack(Current());
    if (LLCC_SystemIsMode(0xe) != 0) LLCC_SystemSetModeNR(0xb, false);
    if (LLCC_SystemIsMode(0x13) != 0) LLCC_SystemSetModeNR(0x10, false);
}

// The track cell of (x, y), or of its parent for a track X cell.
CTrackCell* TrackOrParent(uint16_t index) {
    CTrackCell* cell = TrackCellAt(index);
    if (cell->IsTrackX() != 0) cell = TrackCellAt(cell->mParentID);
    return cell;
}

// The illegal path's jump table (0x1029df4c, 55 words) by GetMode: 0 the exit, 1 SetMode(0), 2 the dead slot stores, 3 the pylon abort.
// (Modes 1, 3, 0xb, 0xe, 0x10, 0x13 and 0x36 end the mode, 4 has the slot stores, 0x16 aborts the pylon move.)
const uint8_t kIllegalCase[0x37] = {
    0, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
};

// The ride placement modes 4 and 0x3b after the prelude (block A): the ride, its track start and its queue start.
void PlaceRideClick(int32_t& x, int32_t& y, uint16_t shift, bool editMode) {
    const int32_t placement = LLCR_PlaceRide(x, y, 4, LLCC_GetRideRotation(), 1, 0);
    LLCR_PRR* const block = reinterpret_cast<LLCR_PRR*>(static_cast<uintptr_t>(static_cast<uint32_t>(placement)));
    if (LLCC_SystemGetRideQueue() == 0) {
        if (shift != 0xc) LLCC_SystemSetModeNR(0, false);
    } else {
        if (LLCC_SystemGetRideTrack() != 0) {
            gLLCCDB4C = LLCR_StartQueue(block, &gLLCC9F28, &gLLCC9F2C);
            if (LLCC_SystemGetRideTrack() != 3) {
                gLLCCDB48 = LLCR_StartTrack(block, &gLLCC4804, &gLLCC4800, LLCC_SystemGetRideTrack());
                const uint16_t start = gLLCCDB48->mCellId;
                int32_t lx = CellX(start);
                int32_t ly = CellY(start);
                LLCC_SystemResetClickStack();
                const int32_t track = LLCC_SystemGetRideTrack();
                if (track == 1)
                    LLCC_SystemSetModeNR(0xb, false);
                else if (track == 2)
                    LLCC_SystemSetModeNR(0x10, false);
                if (!editMode) {
                    DrawTrackLine(0x87, lx, ly, &lx, &ly);
                    DrawTrackLine(LLCC_SystemGetMode(), lx, ly, &lx, &ly);
                    DrawTrackLine(0x80, lx, ly, &lx, &ly);
                    CTrackCell* cell = TrackCellAt(CellIndex(lx, ly));
                    cell->ModifyDirection(DirectionOfRotation(Current()->mRotation));
                    DrawTrackLine(0x83, lx, ly, &lx, &ly);
                    DrawTrackLine(0x85, lx, ly, &lx, &ly);
                    DrawTrackLine(0x86, lx, ly, &lx, &ly);
                    DrawTrackLine(0x81, lx, ly, &lx, &ly);
                } else {
                    DrawTrackLine(0x87, lx, ly, &lx, &ly);
                    DrawTrackLine(LLCC_SystemGetMode(), lx, ly, &lx, &ly);
                    DrawTrackLine(0x80, lx, ly, &lx, &ly);
                    DrawTrackLine(0x82, lx, ly, &lx, &ly);
                    DrawTrackLine(0x83, lx, ly, &lx, &ly);
                    DrawTrackLine(0x85, lx, ly, &lx, &ly);
                    DrawTrackLine(0x86, lx, ly, &lx, &ly);
                    DrawTrackLine(0x81, lx, ly, &lx, &ly);
                }
                LLCC_SystemAddToClickStack(lx, ly);
                gLLCCFirstBlockX = CellX(gLLCCDB68);
                gLLCCFirstBlockY = CellY(gLLCCDB68);
                gLLCCE034 = gLLCCFirstBlockX;
                gLLCCE038 = gLLCCFirstBlockY;
                LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
                LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
                lx = gLLCCE034;
                ly = gLLCCE038;
                uint8_t dir;
                const int32_t dx = gLLCCFirstBlockX - lx;
                if (dx < 0) {
                    dir = 0x40;
                } else if (dx > 0) {
                    dir = 4;
                } else {
                    const int32_t dy = gLLCCFirstBlockY - ly;
                    if (dy < 0)
                        dir = 1;
                    else if (dy > 0)
                        dir = 0x10;
                    else
                        dir = 0;
                }
                DrawTrackLine(0x87, lx, ly, &lx, &ly);
                DrawTrackLine(LLCC_SystemGetMode(), lx, ly, &lx, &ly);
                DrawTrackLine(0x80, lx, ly, &lx, &ly);
                CTrackCell* end = TrackCellAt(CellIndex(lx, ly));
                end->ModifyNeighbours(dir);
                end->ModifyDirection(dir);
                CTrackCell* beyond = end->GetNeighbouringCell(dir, 2);
                if (beyond != nullptr) beyond->ModifyNeighbours(OPPOSITE(dir));
                if (editMode) DrawTrackLine(0x82, lx, ly, &lx, &ly);
                DrawTrackLine(0x83, lx, ly, &lx, &ly);
                DrawTrackLine(0x85, lx, ly, &lx, &ly);
                DrawTrackLine(0x86, lx, ly, &lx, &ly);
                DrawTrackLine(0x81, lx, ly, &lx, &ly);
            }
            CObject* object = Current();
            if (object != nullptr && gLLCCE0DC == 0) {
                object->mF28 = 0;
                gLLCCDB68 = static_cast<uint16_t>(object->mBase.mCellX + (object->mBase.mCellY << 7) + 1);
                gLLCC9F18 = gLLCCFirstBlockX = CellX(gLLCCDB68);
                gLLCC9F1C = gLLCCFirstBlockY = CellY(gLLCCDB68);
                gLLCC9F20 = Current()->mRotation;
                gLLCCE034 = gLLCCFirstBlockX;
                gLLCCE038 = gLLCCFirstBlockY;
                if (LLCC_SystemGetRideTrack() != 3) {
                    gLLCCE0CC = LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
                    LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
                }
            }
        }
        gLLCCDB4C = LLCR_StartQueue(block, &gLLCC4804, &gLLCC4800);
        const uint16_t queue = gLLCCDB4C->mCellId;
        int32_t qx = CellX(queue);
        int32_t qy = CellY(queue);
        LLCC_SystemResetClickStack();
        LLCC_SystemSetModeNR(3, false);
        if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
            LLCC_SystemAddToClickStack(qx, qy);
            if (!editMode && gLLCCDB4C != nullptr) gLLCCDB4C = nullptr;
            DrawLine(0x85, qx, qy, &qx, &qy);
            DrawLine(0x86, qx, qy, &qx, &qy);
            DrawLine(0x83, qx, qy, &qx, &qy);
            DrawLine(0x81, qx, qy, &qx, &qy);
            Current()->RecalculateBackOfQueue();
            gCalcNeighboursPrevA = 0x80;
            gCalcNeighboursPrevB = 0x80;
            if (static_cast<int32_t>(MapCellAt(queue)->mType) == 1) LLCC_SystemSetMode(0, false);
        }
    }
    if (shift != 0xc) LLCC_SetRideRotation(0, false);
    gTrackCostFree = 0;
}

void IllegalPlacement(int32_t x, int32_t y) {
    if (Recording() == 0 && gLLCCDB34 != 0) Sound(0xaf, x, y);
    const uint32_t mode = static_cast<uint32_t>(LLCC_SystemGetMode());
    if (mode > 0x36) return;
    switch (kIllegalCase[mode]) {
    case 1:
        LLCC_SystemSetMode(0, false);
        break;
    case 3:
        AbortMovePylon();
        LLCM_SetIllegalPlacement(0);
        gLLCCE090 = 0;
        break;
    default:
        break;
    }
}

void Modes(int32_t& x, int32_t& y, uint16_t shift, int32_t limitX, int32_t limitY) {
    bool lineDone = false;   // r20

    // ---- paths and queues
    if (LLCC_SystemIsMode(1) != 0 || LLCC_SystemIsMode(3) != 0) {
        if (Recording() == 0) Sound(0x65, x, y);
        if (LLCC_SystemIsMode(1) != 0) gLLCCDE0B8 = 2;
        if (LLCC_SystemIsMode(3) != 0) gLLCCDE0B8 = 3;
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            CMapCell* cell = MapCellAt(CellIndex(x, y));
            if (!(LLCC_SystemIsMode(1) != 0 && cell->ConnectionType(static_cast<CELL_MODES>(1), 0) != 1)) {
                if (LLCC_SystemIsMode(3) == 0) return;
                if (cell->ConnectionType(static_cast<CELL_MODES>(3), 0) == 1) return;
            }
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_SystemResetClickStack();
            LLCC_SystemAddToClickStack(x, y);
            if (LLCC_SystemIsMode(1) != 0) Advise(0xb8);
            gLLCCDB30 = 0;
            return;
        }
        gLLCCDB30 = gLLCCDB30 + 1;
        if (gLLCCDB30 == 3) Advise(0xd6);
        Snap(&x, &y);
        if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
            LLCC_SystemAddToClickStack(x, y);
            DrawLine(0x87, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(LLCC_SystemGetMode(), gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x80, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x82, gLLCC4804, gLLCC4800, &x, &y);
            if (gLLCCDB4C != nullptr) gLLCCDB4C = nullptr;
            DrawLine(0x85, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x83, gLLCC4804, gLLCC4800, &x, &y);
            lineDone = DrawLine(0x81, gLLCC4804, gLLCC4800, &x, &y) == 0;
        }
        if (LLCC_SystemIsMode(3) != 0) {
            Current()->RecalculateBackOfQueue();
            if (Recording() == 0 && gLLCCC208 != 0) Sound(0x8b, x, y);
        }
        if ((gLLCC4804 == x && gLLCC4800 == y) || lineDone) {
            gLLCCDB40 = 0;
            EndDragWords();
            const bool queue = LLCC_SystemIsMode(3) != 0;
            LLCC_SystemSetMode(0, false);
            if (queue && Recording() == 0 && gLLCCC208 == 0) Advise(0x151);
            return;
        }
        gLLCC4804 = x;
        gLLCC4800 = y;
        return;
    }

    // ---- ride placement
    if (LLCC_SystemIsMode(4) != 0) {
        gTrackCostFree = 1;
        gLLCCDB4C = nullptr;
        gLLCCDB48 = nullptr;
        gLLCCDB44 = 0;
        if (gLLCCE0DC != 0) {
            LLCM_SetCurrentObject(false);
            CTrackCell* cell = TrackCellAt(CellIndex(x, y));
            if (cell->IsTrack() == 0 && cell->IsTrackX() == 0) {
                CTRACE(6, "How'd we get here?\n");
            } else {
                if (cell->IsTrackX() != 0) cell = TrackCellAt(cell->mParentID);
                CMapCell* mapCell = TrackCellAt(cell->mParentID)->GetMapCellEquiv();
                const uint16_t id = mapCell->GetObjectThingId().mIndex;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                SetCurrent(static_cast<CObject*>(gThingArray[id].mThing));
                BMP_FreeTrack(Current()->mF24);
                LLCR_PlaceRide(x, y, 4, LLCC_GetRideRotation(), 1, 0);
                LLCM_RebuildUpgradeBridges(x, y);
                CObject* object = Current();
                gLLCCDB68 = static_cast<uint16_t>(object->mBase.mCellX + (object->mBase.mCellY << 7) + 1);
                gLLCC9F18 = gLLCCFirstBlockX = CellX(gLLCCDB68);
                gLLCC9F1C = gLLCCFirstBlockY = CellY(gLLCCDB68);
                gLLCC9F20 = Current()->mRotation;
                gLLCCE034 = gLLCCFirstBlockX;
                gLLCCE038 = gLLCCFirstBlockY;
                gLLCCE0CC = LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
                LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
                const int32_t whole = LLCC_BuildTrackDB(false);
                Current()->mF28 = whole;
                if (whole != 0) {
                    LLCC_SetTrackAnim(LLCM_GetCurrentObject(), true);
                } else {
                    LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
                    LLCC_SetTrackAnimReverse(LLCM_GetCurrentObject(), false);
                }
                if (whole != 0 && Current()->IsOpenable() != 0) Current()->Reopen();
            }
            if (shift != 0xc) {
                LLCC_SystemSetModeNR(0, false);
                gLLCCE0DC = 0;
            }
            LLCC_SetRideRotation(0, false);
            gTrackCostFree = 0;
            return;
        }
        PlaceRideClick(x, y, shift, false);
        return;
    }
    if (LLCC_SystemIsMode(0x3b) != 0) {
        gTrackCostFree = 1;
        gLLCCDB4C = nullptr;
        gLLCCDB48 = nullptr;
        gLLCCDB44 = 0;
        PlaceRideClick(x, y, shift, true);
        return;
    }

    // ---- track laying
    if (LLCC_SystemIsMode(0xb) != 0 || LLCC_SystemIsMode(0x10) != 0) {
        if (Recording() == 0) Sound(0xce, x, y);
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_SystemResetClickStack();
            LLCC_SystemAddToClickStack(x, y);
            Advise(0xcc);
            return;
        }
        Snap(&x, &y);
        if (gLLCC4804 != x || gLLCC4800 != y || (gLLCCFirstBlockX == x && gLLCCFirstBlockY == y)) {
            DrawTrackLine(0x87, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(LLCC_SystemGetMode(), gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x80, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x82, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x83, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x85, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
            lineDone = DrawTrackLine(0x81, gLLCC4804, gLLCC4800, &x, &y) == 0;
            LLCC_SystemAddToClickStack(x, y);
        }
        if ((gLLCC4804 == x && gLLCC4800 == y) || lineDone) {
            const int32_t whole = LLCC_BuildTrackDB(false);
            Current()->mF28 = whole;
            if (whole != 0 && Current()->IsOpenable() != 0) Current()->Reopen();
            LLCR_FixBridges(Current(), true, false, false);
            LLCR_FixBridgesReverse(Current(), false, false, false);
            if (whole != 0) {
                LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
            } else {
                LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
                LLCC_SetTrackAnimReverse(LLCM_GetCurrentObject(), false);
            }
            EndDragWords();
            if (gLLCCDB4C == nullptr) {
                if (shift != 0xc) LLCC_SystemSetMode(0, false);
                return;
            }
            gLLCC4804 = gLLCC9F28;
            gLLCC4800 = gLLCC9F2C;
            gLLCCDB5A = 0;
            gLLCC9F24 = 0xffff;
            gLLCCDB58 = 0;
            gLLCC9F26 = 0xffff;
            gLLCC9F14 = -1;
            gLLCCE0DC = 0;
            gLLCC47C8 = 0;
            LLCC_SystemResetClickStack();
            LLCC_SystemSetModeNR(3, false);
            if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
                LLCC_SystemAddToClickStack(gLLCC9F28, gLLCC9F2C);
                if (gLLCCDB4C != nullptr) gLLCCDB4C = nullptr;
                DrawLine(0x85, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                DrawLine(0x86, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                DrawLine(0x83, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                DrawLine(0x81, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                gCalcNeighboursPrevA = 0x80;
                gCalcNeighboursPrevB = 0x80;
                if (static_cast<int32_t>(MapCellAt(CellIndex(gLLCC9F28, gLLCC9F2C))->mType) == 1) LLCC_SystemSetMode(0, false);
            }
            return;
        }
        gLLCC4804 = x;
        gLLCC4800 = y;
        if (Current() != nullptr) {
            LLCR_FixBridges(Current(), false, false, false);
            LLCR_FixBridgesReverse(Current(), true, false, false);
        }
        return;
    }

    // ---- hoardings
    if (LLCC_SystemIsMode(0x19) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_SystemResetClickStack();
            LLCC_SystemAddToClickStack(x, y);
            return;
        }
        Snap(&x, &y);
        const int32_t startX = gLLCC4804;
        if (startX != x || gLLCC4800 != y || (gHoardingRight == x && gHoardingBottom == y)) {
            const int32_t mode = LLCC_SystemGetMode();
            DrawTrackLine(mode, startX, gLLCC4800, &x, &y);
            DrawTrackLine(0x80, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x82, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x85, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x81, gLLCC4804, gLLCC4800, &x, &y);
            LLCC_SystemAddToClickStack(x, y);
        }
        if (gLLCC4804 == x && gLLCC4800 == y) {
            EndDragWords();
            LLCC_SystemSetMode(0, false);
        } else {
            gLLCC4804 = x;
            gLLCC4800 = y;
        }
        return;
    }
    if (LLCC_SystemIsMode(0x39) != 0) {
        if (x < limitX) x = limitX;
        if (y < limitY) y = limitY;
        const int32_t right = gLLCCWidth - 2;
        if (right < x) x = right;
        const int32_t bottom = gLLCCHeight - 3;
        if (bottom < y) y = bottom;
        LLCM_TrackCellBoundary(gHoardingRight, gHoardingBottom, &x, &y, &x, &y);
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_CreateCurrentHoardingImage();
            return;
        }
        if (LLCC_IsHoardingEditLegal(gLLCC4804, gLLCC4800, x, y, true) != 0) {
            LLCC_ReviseHoarding(gLLCC4804, gLLCC4800, x, y);
            const int32_t cost = LLCC_SystemGetSessionCost();
            gGameWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(cost));
        }
        gLLCC4804 = -1;
        gLLCC4800 = -1;
        LLCC_SystemSetMode(0, false);
        UI_SetGameWindowCursor(0);
        return;
    }

    // ---- land, deletion, lines, patrol areas
    if (LLCC_SystemIsMode(0x3a) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            return;
        }
        if (LLCC_IsClearLandLegal(gLLCC4804, gLLCC4800, x, y) != 0) LLCC_Kill_Them_KILL_THEM_ALL(gLLCC4804, gLLCC4800, x, y);
        gLLCC4804 = -1;
        gLLCC4800 = -1;
        LLCC_SystemSetMode(0, false);
        return;
    }
    if (LLCC_SystemIsMode(0x33) != 0) {
        gLLCCDB4C = nullptr;
        gLLCCDB48 = nullptr;
        gLLCCDB44 = 0;
        LLCC_SetRideRotation(0, false);
        LLCR_DeleteRide(x, y, 0);
        return;
    }
    if (LLCC_SystemIsMode(0x32) != 0) {
        if (static_cast<int32_t>(MapCellAt(CellIndex(x, y))->mType) != 4) {
            DrawLine(0x32, x, y, &x, &y);
            DrawLine(0x86, x, y, &x, &y);
        }
        return;
    }
    if (LLCC_SystemIsMode(0x34) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            return;
        }
        DrawLine(0x32, gLLCC4804, gLLCC4800, &x, &y);
        DrawLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
        gLLCC4804 = x;
        gLLCC4800 = y;
        return;
    }
    if (LLCC_SystemIsMode(0x35) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            return;
        }
        DrawTrackLine(0x32, gLLCC4804, gLLCC4800, &x, &y);
        gLLCC4804 = x;
        gLLCC4800 = y;
        return;
    }
    if (LLCC_SystemIsMode(0x36) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            gLLCCDB64 = gUIInterface.GetSelectedPeep();
            return;
        }
        if (gLLCCDB64 != nullptr) {
            const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
            const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
            const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
            const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> low;
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> high;
            low.mIndex = CellIndex(minX, minY);
            high.mIndex = CellIndex(maxX, maxY);
            reinterpret_cast<CStaff*>(gLLCCDB64)->SetPatrolArea(low, high);
        }
        gLLCC4804 = -1;
        gLLCC4800 = -1;
        LLCC_SystemSetMode(0, false);
        return;
    }

    // ---- queue and track editing
    if (LLCC_SystemIsMode(0x14) != 0) {
        LLCC_SystemConstructQueueClickStack(Current());
        Current()->RecalculateBackOfQueue();
        LLCC_SystemSetModeNR(3, false);
        return;
    }
    if (LLCC_SystemIsMode(0xe) != 0 || LLCC_SystemIsMode(0x13) != 0) {
        bool fixBridges = true;   // r13
        if (Current()->mF28 == 0) {
            TrackStackAgain();
        } else if (static_cast<int16_t>(gLLCC9F24) == -1) {
            CTrackCell* cell = TrackOrParent(CellIndex(x, y));
            if (cell != nullptr && cell->IsTrackStartBlock() == 0) {
                int32_t coverX = x;
                int32_t coverY = y;
                LLCM_CoverUpgrade(&coverX, &coverY);
                const int16_t here = cell->GetSegmentNumber();
                const int16_t covered = TrackCellAt(CellIndex(coverX, coverY))->GetSegmentNumber();
                if (covered < here) {
                    gLLCCDB54 = coverX;
                    gLLCCDB50 = coverY;
                }
                gLLCCDB5A = CellIndex(x, y);
                gLLCC9F24 = static_cast<uint16_t>(cell->GetSegmentNumber());
                if (cell->ConnectionType(static_cast<CELL_MODES>(0xe), 0, 0) == 1) {
                    gLLCCDB5A = 0;
                    gLLCC9F24 = 0xffff;
                }
                const int32_t whole = LLCC_BuildTrackDB(true);
                Current()->mF28 = whole;
                if (whole != 0 && Current()->IsOpenable() != 0) Current()->Reopen();
                fixBridges = false;
                if (whole == 0) TrackStackAgain();
            }
        } else {
            CTrackCell* cell = TrackOrParent(CellIndex(x, y));
            if (cell != nullptr && cell->IsTrackStartBlock() == 0) {
                int32_t coverX = x;
                int32_t coverY = y;
                LLCM_CoverUpgrade(&coverX, &coverY);
                const int16_t here = cell->GetSegmentNumber();
                const int16_t covered = TrackCellAt(CellIndex(coverX, coverY))->GetSegmentNumber();
                if (covered > here) {
                    gLLCCDB54 = coverX;
                    gLLCCDB50 = coverY;
                }
                gLLCCDB58 = CellIndex(x, y);
                gLLCC9F26 = static_cast<uint16_t>(TrackCellAt(gLLCCDB58)->GetSegmentNumber());
                CTrackCell* end = TrackOrParent(gLLCCDB58);
                if (end->ConnectionType(static_cast<CELL_MODES>(0xe), 0, 0) != 9) {
                    LLCC_SystemSetMode(0, false);
                } else {
                    if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26))
                        TraceTrack(TrackCellAt(gLLCCDB58), TrackCellAt(gLLCCDB5A), 1);
                    else
                        TraceTrack(TrackCellAt(gLLCCDB5A), TrackCellAt(gLLCCDB58), 1);
                    LLCC_FixFirstBlock(false);
                    if (LLCR_FixBridges(Current(), true, false, false) == 0) LLCR_FixBridgesReverse(Current(), false, false, false);
                    gLLCCDB5A = 0;
                    gLLCC9F24 = 0xffff;
                    gLLCCDB58 = 0;
                    gLLCC9F26 = 0xffff;
                    gLLCC4804 = CellX(end->mCellId);
                    gLLCC4800 = CellY(end->mCellId);
                    const int32_t whole = LLCC_BuildTrackDB(false);
                    Current()->mF28 = whole;
                    if (whole != 0) {
                        LLCC_SetTrackAnim(LLCM_GetCurrentObject(), true);
                    } else {
                        LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
                        LLCC_SetTrackAnimReverse(LLCM_GetCurrentObject(), false);
                    }
                    if (whole == 0) {
                        TrackStackAgain();
                    } else {
                        if (Current()->IsOpenable() != 0) Current()->Reopen();
                        LLCC_SystemSetModeNR(0, false);
                    }
                }
            }
        }
        if (fixBridges) {
            LLCR_FixBridges(Current(), false, false, false);
            LLCR_FixBridgesReverse(Current(), true, false, false);
        }
        return;
    }
    if (LLCC_SystemIsMode(0x1a) != 0) {
        LLCM_TrackCellBoundary(gLLCC9F18, gLLCC9F1C, &x, &y, &gLLCCDB54, &gLLCCDB50);
        if (static_cast<int16_t>(gLLCC9F24) == -1) {
            gLLCCDB5A = CellIndex(gLLCCDB54, gLLCCDB50);
            gLLCC9F24 = static_cast<uint16_t>(TrackCellAt(gLLCCDB5A)->GetSegmentNumber());
            if (TrackCellAt(gLLCCDB5A)->ConnectionType(static_cast<CELL_MODES>(0x1a), 0, 0) == 1) {
                gLLCCDB5A = 0;
                gLLCC9F24 = 0xffff;
            }
            return;
        }
        gLLCCDB58 = CellIndex(gLLCCDB54, gLLCCDB50);
        gLLCC9F26 = static_cast<uint16_t>(TrackCellAt(gLLCCDB58)->GetSegmentNumber());
        CTrackCell* end = TrackOrParent(gLLCCDB58);
        if (end->ConnectionType(static_cast<CELL_MODES>(0x1a), 0, 0) != 9) {
            gLLCCDB58 = gLLCCDB5A;
            gLLCC9F26 = gLLCC9F24;
            return;
        }
        int32_t traced;
        if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26))
            traced = TraceTrack(TrackCellAt(gLLCCDB58), TrackCellAt(gLLCCDB5A), 1);
        else
            traced = TraceTrack(TrackCellAt(gLLCCDB5A), TrackCellAt(gLLCCDB58), 1);
        gLLCCDB5A = 0;
        gLLCC9F24 = 0xffff;
        gLLCCDB58 = 0;
        gLLCC9F26 = 0xffff;
        gLLCC4804 = CellX(end->mCellId);
        gLLCC4800 = CellY(end->mCellId);
        gLLCCReviseAnswer = 0;
        if (traced != -1) {
            LLCC_SystemConstructHoardingClickStack();
            LLCC_SystemSetModeNR(0x19, false);
        }
        return;
    }

    // ---- coaster pylons
    if (LLCC_SystemIsMode(0x15) != 0) {
        if (Recording() == 0) Sound(0x65, x, y);
        CoasterAcceptLeftMouseClick(0);
        Advise(0xcd);
        LLCC_SystemAddToClickStack(x, y);
        DrawLine(0x87, x, y, &x, &y);
        DrawLine(LLCC_SystemGetMode(), x, y, &x, &y);
        DrawLine(0x83, x, y, &x, &y);
        DrawLine(0x84, x, y, &x, &y);
        if (DrawLine(0x81, x, y, &x, &y) != 0) {
            PlacePylon(static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0, &TrackCellAt(CellIndex(x, y))->mMeshInstance);
            return;
        }
        EndTrack(0);
        if (gLLCCDB4C == nullptr) {
            LLCC_SystemSetMode(0, false);
            return;
        }
        LLCC_SystemSetModeNR(3, false);
        gLLCC4804 = gLLCC9F28;
        gLLCC4800 = gLLCC9F2C;
        if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
            int32_t lx = gLLCC4804;
            int32_t ly = gLLCC4800;
            const uint16_t index = CellIndex(lx, ly);
            LLCC_SystemAddToClickStack(lx, ly);
            DrawLine(0x85, lx, ly, &lx, &ly);
            DrawLine(0x86, lx, ly, &lx, &ly);
            DrawLine(0x83, lx, ly, &lx, &ly);
            DrawLine(0x81, lx, ly, &lx, &ly);
            gCalcNeighboursPrevA = 0x80;
            gCalcNeighboursPrevB = 0x80;
            gLLCCDB4C = nullptr;
            if (static_cast<int32_t>(MapCellAt(index)->mType) == 1) LLCC_SystemSetMode(0, false);
        }
        return;
    }
    if (LLCC_SystemIsMode(0x16) != 0) {
        if (gLLCCE090 != 0) {
            LLCC_SystemAddToClickStack(x, y);
            DrawLine(0x87, x, y, &x, &y);
            DrawLine(0x15, x, y, &x, &y);
            DrawLine(0x83, x, y, &x, &y);
            DrawLine(0x81, x, y, &x, &y);
            const uint16_t index = CellIndex(x, y);
            const uint32_t result = DoMovePylon(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
            TrackCellAt(index)->mMeshInstance = result;
            gLLCC9F14 = -1;
            gLLCCE090 = 0;
        } else {
            CoasterAcceptLeftMouseClick(0);
        }
    }
}

}  // namespace

void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) {
    const int32_t limitY = gLLCC9F48;
    const int32_t limitX = gLLCC9F44;
    if (x == 0 && y == 0) CTRACE(6, "HUH?\n");
    if (Recording() != 0 && gLLCC4818 == 0) {
        gLLCCC214 = 0;
        if (gLLCCE0DC == 0) LLCM_MouseMoved(x, y, shift);
    }
    gTrackCostFree = 0;
    if (flag != 0) {
        x = gLLCCDB54;
        y = gLLCCDB50;
    }
    if ((LLCM_GetIllegalPlacement() == 0 && gLLCC4818 == 0) || LLCC_SystemIsMode(0x16) != 0) {
        if (shift == 0)
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0x14), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        else
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0x2d), static_cast<uint8_t>(x), static_cast<uint8_t>(y), shift);
    }
    if (LLCC_SystemIsMode(0) != 0) gLLCCDB64 = gUIInterface.GetSelectedPeep();
    if (LLCC_SystemIsMode(0x38) == 0 && LLCM_GetIllegalPlacement() != 0) {
        IllegalPlacement(x, y);
    } else {
        LLCM_SetCurrentObject(false);
        if (LLCC_SystemIsMode(0) == 0) Modes(x, y, shift, limitX, limitY);
    }
    gLLCCDB34 = 0;
}
