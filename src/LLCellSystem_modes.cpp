// The cell system's modes, costs, ride words, cursor, click stack and first block fix up (main app, LLCellSystem.cpp range 0x10090d40 to
// 0x10092250, SimTheme Park 2.04b3; unit w185). The stores and calls keep the listing's order: the logs compare them. The globals follow the
// header's rule (named after their addresses; the click stack and the first block words have invented names). Every compare of a mode is signed
// except the cursor's jump table (cmplwi 0x3b).
#include <cstdint>
#include <new>

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "ui_main.h"

// New words of the unit, with their image values (initialised data on the Mac).
int32_t gLLCC9F14 = -1;       // 0x1029df14
uint16_t gLLCC9F24 = 0xffff;  // 0x1029df24
uint16_t gLLCC9F26 = 0xffff;  // 0x1029df26
int32_t gLLCCE288 = 0x80;     // 0x1029e288
int32_t gLLCCE28C = 0x80;     // 0x1029e28c
uint16_t gLLCCDB58 = 0;       // 0x102ddb58
uint16_t gLLCCDB5A = 0;       // 0x102ddb5a
int32_t gLLCCE0DC = 0;        // 0x102de0dc  the selected ride's track
uint16_t gLLCCE0E0 = 0;       // 0x102de0e0  the selected ride's id
int32_t gLLCCFirstBlockX = -1;   // 0x1029e02c  (name invented)
int32_t gLLCCFirstBlockY = -1;   // 0x1029e030  (name invented)

// The click stack (names invented): the Mac keeps the copy, the copy's index and count words, the stack and the stack's count in one run of
// memory (0x102e07b8 to 0x102e47c4), which only matters for the overflow of LLCC_SystemAddToClickStack.
LLCCClick gLLCCClickStack[0x400];       // 0x102e27c0
LLCCClick gLLCCClickStackCopy[0x400];   // 0x102e07b8
int32_t gLLCCClickCopyIndex = 0;        // 0x102e27b8
int32_t gLLCCClickCopyCount = 0;        // 0x102e27bc

namespace {

// The track cell of a 1 based id: plain arithmetic on the world, as the Mac does it (id 0 lands one cell before the first).
CTrackCell* TrackCellOfId(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// The ten advisor messages a mode change cancels, in the Mac's order; the advisor is fetched again before each.
void CancelBuildMessages() {
    static const int32_t messages[10] = {0xb8, 0xcb, 0xd6, 0xcd, 0xc9, 0xce, 0xc1, 0x159, 0x15a, 0x151};
    for (int32_t i = 0; i < 10; ++i) {
        gGameWorld->GetAdvisor()->CancelMessage(static_cast<AdvisorMessage>(messages[i]));
    }
}

// The mouse and its default mode (the same idiom as src/Mouse_modes.cpp's, written again): operator new(1) for the mouse, operator new(8) for
// the mode, which the mouse takes; the two words of the mouse's data are set to -1 only when the mouse was made.
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}

// Puts a new default mouse mode in place (made first, then the mouse if there is none). The Mac calls the assignment even when the mouse could
// not be made (this is NULL then).
void PutDefaultMouseMode() {
    CMMDefault* mode = new (std::nothrow) CMMDefault;
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);
}

// The arms SetMode and SetModeNR share (3, 0x15, 0x16, 1). Answers whether the mode was one of them.
bool ModeArm(int32_t mode) {
    if (mode == 3) {
        // The request is built before the advisor is fetched; the thing id is 0 (the Mac passes a word with a zero halfword in its high half).
        CAdvisorRequest request(static_cast<AdvisorMessage>(0xcb), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1);
        gGameWorld->GetAdvisor()->QueueRequest(request);
        UI_SetGameWindowCursor(4);
        return true;
    }
    if (mode == 0x15 || mode == 0x16) {
        // The Mac passes the word at 0x102ddb6c as a pointer; here the int32 is widened without sign extension (the unit's log keeps 32 bits).
        SetCoasterEdit(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C))), 1);
        return true;
    }
    if (mode == 1) {
        UI_SetGameWindowCursor(3);
        return true;
    }
    return false;
}

}  // namespace

// 0x10090d40 (116 bytes). The compare is with the LAST rotation (gLLCC4810), not the current one, as the listing reads (a doubt).
void LLCC_SetRideRotation(int32_t rotation, bool flag) {
    if (rotation != gLLCC4810 || flag) {
        gActionRecorder.RecordAction(10, static_cast<uint16_t>(rotation));
    }
    gLLCC4810 = gLLCC4814;
    gLLCC4814 = rotation;
}

// 0x10090de0 (12 bytes).
int32_t LLCC_GetRideRotation() { return gLLCC4814; }

// 0x10090e18 (928 bytes).
void LLCC_SystemSetMode(int32_t mode, bool flag) {
    if (flag) gActionRecorder.RecordAction(0, static_cast<uint16_t>(mode));
    if (gLLCC480C != mode) CancelBuildMessages();
    gLLCC4808 = gLLCC480C;
    gLLCC480C = mode;
    gLLCC4804 = -1;
    gLLCC4800 = -1;
    if (mode == 0) {
        gLLCCE288 = 0x80;
        gLLCCE28C = 0x80;
        gLLCCDB5A = 0;
        gLLCC9F24 = 0xffff;
        gLLCCDB58 = 0;
        gLLCC9F26 = 0xffff;
        gLLCC9F14 = -1;
        gLLCC47C8 = 0;
        LLCC_SetRideRotation(0, false);   // inlined on the Mac
        if (gLLCC4808 != 0x15 && gLLCC4808 != 0x16) PutDefaultMouseMode();
        AbortMovePylon();
        LLCM_SetIllegalPlacement(0);
    } else if (!ModeArm(mode) && mode == 0x35) {
        gLLCCE288 = 0x80;
        gLLCCE28C = 0x80;
    }
}

// 0x100911f0 (716 bytes). Differs from SetMode: the action is 0x19; only the last mode and the mode are stored (not the two -1 words); mode 0 only
// puts the default mouse mode back (with the same 0x15 / 0x16 test); there is no 0x35 arm.
void LLCC_SystemSetModeNR(int32_t mode, bool flag) {
    if (flag) gActionRecorder.RecordAction(0x19, static_cast<uint16_t>(mode));
    if (gLLCC480C != mode) CancelBuildMessages();
    gLLCC4808 = gLLCC480C;
    gLLCC480C = mode;
    if (mode == 0) {
        if (gLLCC4808 != 0x15 && gLLCC4808 != 0x16) PutDefaultMouseMode();
    } else {
        ModeArm(mode);
    }
}

// 0x1009152c (12 bytes).
int32_t LLCC_SystemGetLastMode() { return gLLCC4808; }

// 0x10091568 (32 bytes).
int32_t LLCC_SystemIsMode(int32_t mode) { return gLLCC480C == mode ? 1 : 0; }

// 0x100915bc (144 bytes). The id is read as a halfword for the record and as the high half of a word for FlagItemAsSelected.
void LLCC_SystemSetRide(const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id, int32_t track) {
    gActionRecorder.RecordAction(5, id.mIndex, static_cast<uint8_t>(track));
    gGameWorld->FlagItemAsSelected(id);
    gLLCCDE0E2 = gLLCCE0E0;
    gLLCCE0E0 = id.mIndex;
    gLLCCE0DC = track;
}

// 0x1009179c (460 bytes). The switch is on the unsigned mode (cmplwi 0x3b, jump table 0x1029e11c): every other value does nothing before the
// two trailing tests.
void LLCC_SystemSetCursor() {
    switch (static_cast<uint32_t>(gLLCC480C)) {
        case 1:
            UI_SetGameWindowCursor(gLLCCC208 != 0 ? 5 : gLLCCC200 != 0 ? 0x12 : 3);
            break;
        case 3:
            UI_SetGameWindowCursor(gLLCCC208 != 0 ? 5 : gLLCCC200 != 0 ? 0x12 : 4);
            break;
        case 4:
        case 59:
            UI_SetGameWindowCursor(7);
            break;
        case 11:
        case 16:
            UI_SetGameWindowCursor(gLLCCC208 != 0 ? 5 : gLLCCC200 != 0 ? 0x12 : 6);
            break;
        case 21:
            UI_SetGameWindowCursor(gLLCCC200 != 0 ? 0x12 : 0xb);
            break;
        case 22:
            UI_SetGameWindowCursor(gLLCCC200 != 0 ? 0x12 : 0x11);
            break;
        case 57:
            UI_SetGameWindowCursor(3);
            break;
        default:
            break;
    }
    if (LLCM_GetIllegalPlacement() != 0) UI_SetGameWindowCursor(8);
    if (gLLCCC204 != 0) UI_SetGameWindowCursor(0x14);
}

// 0x10091994 (68 bytes). The record comes before the store.
void LLCC_SystemSetRideQueue(int32_t queue) {
    gActionRecorder.RecordAction(0x23, static_cast<uint16_t>(queue));
    gLLCC47EC = queue;
}

// 0x10091a08 (36 bytes).
int32_t LLCC_SystemGetRideQueue() { return gLLCCE0DC != 0 ? 0 : gLLCC47EC; }

// 0x10091a5c (68 bytes). The record comes before the store.
void LLCC_SystemSetRideTrack(int32_t track) {
    gActionRecorder.RecordAction(0x1e, static_cast<uint16_t>(track));
    gLLCC47E8 = track;
}

// 0x10091ad0 (36 bytes).
int32_t LLCC_SystemGetRideTrack() { return gLLCCE0DC != 0 ? 0 : gLLCC47E8; }

// 0x10091b60, 0x10091bd8, 0x10091c54, 0x10091cd4 (12 bytes each): the four cost words (set by src/LLCellSystem_costs.cpp).
int32_t LLCC_SystemGetPathCost() { return gLLCC47DC; }
int32_t LLCC_SystemGetQueueCost() { return gLLCC47D8; }
uint32_t LLCC_SystemGetKartTrackCost() { return static_cast<uint32_t>(gLLCC47D4); }
uint32_t LLCC_SystemGetWaterTrackCost() { return static_cast<uint32_t>(gLLCC47D0); }

// 0x10091d14 (12 bytes), 0x10091d50 (20 bytes, wraps), 0x10091d98 (12 bytes).
void LLCC_SystemSetSessionCost(int32_t cost) { gLLCC47CC = cost; }
void LLCC_SystemAddToSessionCost(int32_t cost) { gLLCC47CC = static_cast<int32_t>(static_cast<uint32_t>(gLLCC47CC) + static_cast<uint32_t>(cost)); }
int32_t LLCC_SystemGetSessionCost() { return gLLCC47CC; }

// 0x10091e18 (12 bytes).
int32_t LLCC_SystemGetClickStackEntries() { return gLLCCClickStackCount; }

// 0x10091e5c (64 bytes). The count is compared signed and only a count ABOVE 0x400 is refused, so the entry 0x400 is written one past the array:
// on the Mac x lands in the count word itself and y in the word after it, whose first (high) byte is gLLCC47C4; the count then becomes x + 1.
// The other three bytes of that word are not modelled, nor is a negative count.
int32_t LLCC_SystemAddToClickStack(int32_t x, int32_t y) {
    const int32_t count = gLLCCClickStackCount;
    if (count > 0x400) return 0;
    if (count == 0x400) {
        gLLCCClickStackCount = x;
        gLLCC47C4 = static_cast<uint8_t>(static_cast<uint32_t>(y) >> 24);
    } else {
        gLLCCClickStack[count].mX = x;
        gLLCCClickStack[count].mY = y;
    }
    gLLCCClickStackCount = gLLCCClickStackCount + 1;
    return 1;
}

// 0x10091ed0 (88 bytes). The count is read again on every pass; a count above 0x400 is not modelled (the Mac runs on over the neighbouring words).
void LLCC_SystemMakeClickStackCopy() {
    for (int32_t i = 0; i < gLLCCClickStackCount; ++i) {
        gLLCCClickStackCopy[i] = gLLCCClickStack[i];
    }
    gLLCCClickCopyCount = gLLCCClickStackCount;
    gLLCCClickCopyIndex = 0;
}

// 0x10091f5c (760 bytes). Loop 1 joins the first block's cell to a start block of the same track on each free side and cuts a side whose neighbour
// is not a track (or does not point back); with `flag` and a corner, loop 2 cuts the sides whose neighbour is not a start block (both ways) and for
// a start block resets the cell's direction to the way back. Loop 1 has no null check on the neighbour of a set side (the Mac calls IsTrack on it).
void LLCC_FixFirstBlock(bool flag) {
    if (CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY) == 0) return;
    const uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(gLLCCFirstBlockX) + (static_cast<uint32_t>(gLLCCFirstBlockY) << 7) + 1);
    CTrackCell* cell = TrackCellOfId(id);
    if (cell->IsTrack() == 0) return;
    uint8_t dir = 1;
    do {
        if ((cell->GetNeighbours() & dir) == 0) {
            CTrackCell* neighbour = cell->GetNeighbouringCell(dir, 2);
            if (neighbour != nullptr && neighbour->IsTrackStartBlock() != 0 && neighbour->mParentID == cell->mParentID) {
                cell->ModifyDirection(OPPOSITE(dir));
                cell->ModifyNeighbours(dir);
                cell->SetHFieldTile();
            }
        } else {
            CTrackCell* neighbour = cell->GetNeighbouringCell(dir, 2);
            bool cut = false;
            if (neighbour->IsTrack() == 0 && neighbour->IsTrackStartBlock() == 0) {
                cut = true;
            } else if (neighbour->IsTrack() != 0) {
                const uint8_t opposite = OPPOSITE(dir);
                if ((neighbour->GetNeighbours() & opposite) == 0) cut = true;
            }
            if (cut) {
                cell->RemoveNeighbour(dir);
                cell->SetHFieldTile();
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);
    if (!flag) return;
    if (cell->IsCorner() == 0) return;
    dir = 1;
    do {
        if ((cell->GetNeighbours() & dir) != 0) {
            CTrackCell* neighbour = cell->GetNeighbouringCell(dir, 2);
            if (neighbour != nullptr) {
                if (neighbour->IsTrackStartBlock() == 0) {
                    cell->RemoveNeighbour(dir);
                    cell->SetHFieldTile();
                    neighbour->RemoveNeighbour(OPPOSITE(dir));
                    neighbour->SetHFieldTile();
                } else {
                    cell->RemoveDirection(0xff);
                    cell->ModifyDirection(OPPOSITE(dir));
                }
            }
        }
        dir = ADD_DIRECTION(dir, 4);
    } while (dir != 1);
}
