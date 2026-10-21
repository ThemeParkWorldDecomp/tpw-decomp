// LLCellSystem.cpp: name proven by __sinit_LLCellSystem_cpp in the main application.

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "HoardingsBalance.h"
#include "intrface.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "Mouse.h"
#include "ObjectParts.h"
#include "ppc_math.h"
#include "rsse.h"
#include "sprite_engine.h"
#include "tb_resource_display.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "ThingReceivers.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// The cell system's start and end (main application, LLCellSystem.cpp; unit w111). The stores keep the listing's order: the
// callees (the map reset, the mesh loaders, the hoarding) see the words set so far.

uint8_t gLLCC4818;
uint8_t gLLCC47C4;
int32_t gLLCCWidth;
int32_t gLLCCHeight;
int32_t gLLCC47CC;
int32_t gLLCC4814;
int32_t gLLCC4810;
int32_t gLLCC480C;
int32_t gLLCC4808;
int32_t gLLCC4804;
int32_t gLLCC4800;
uint16_t gLLCCDE0E2;
int32_t gLLCCC214;
int32_t gLLCCC210;
int32_t gLLCCC208;
int32_t gLLCCC204;
int32_t gLLCCC200;
int32_t gLLCC47EC;
int32_t gLLCC47E8;
int32_t gLLCC47E4;
int32_t gLLCC47E0;
int32_t gLLCCDB6C;
uint16_t gLLCCDB68;
int32_t gLLCC47C8;

// 0x10090b68 (332 bytes).
void LLCC_SystemInit(int32_t width, int32_t height) {
    gLLCC4818 = 0;
    gLLCC47C4 = 0;
    gLLCCWidth = width;
    gLLCCHeight = height;
    gLLCC47CC = 0;
    gLLCC4814 = 0;
    gLLCC4810 = 0;
    gLLCC480C = 0;
    gLLCC4808 = 0;
    gLLCC4804 = -1;
    gLLCC4800 = -1;
    gLLCCDE0E2 = 0;
    LLCC_ResetMap(width, height);
    LLCM_SetIllegalPlacement(0);
    gLLCCC214 = 0;
    gLLCCC210 = 0;
    gLLCCC208 = 0;
    gLLCCC204 = 0;
    gLLCCC200 = 0;
    gLLCC47EC = 1;
    gLLCC47E8 = 0;
    gLLCC47E4 = 0;
    gLLCC47E0 = 0;
    LLCM_LoadQMeshes();
    LLCM_LoadKTrackMeshes();
    LLCM_LoadWTrackMeshes();
    LLCM_LoadHoardingMeshes();
    LLCC_SystemResetClickStack();
    LLCC_ResetBuyLandList();
    gLLCCDB6C = 0;
    gLLCCDB68 = 0;
    LLCC_DrawInitialHoarding();
    gLLCC47C8 = 0;
}

// 0x10090cdc (60 bytes).
void LLCC_SystemDeInit() {
    LLCM_DestroyQMeshes();
    LLCM_DestroyKTrackMeshes();
    LLCM_DestroyWTrackMeshes();
    LLCM_DestroyHoardingMeshes();
}

// The cell system's modes, costs, ride words, cursor, click stack and first block fix up (main app, LLCellSystem.cpp range 0x10090d40 to
// 0x10092250, SimTheme Park 2.04b3; unit w185). The stores and calls keep the listing's order: the logs compare them. The globals follow the
// header's rule (named after their addresses; the click stack and the first block words have invented names). Every compare of a mode is signed
// except the cursor's jump table (cmplwi 0x3b).

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

// The dynamic cell meshes (main application; unit w112b). The Mac files are analyser.cpp for the class (CDynamicMesh: constructor
// 0x100e1610, destructor 0x100e191c, SetBoundingBox 0x100e19b0, DC_SetVertex 0x100e1f10, DC_CreateSquareFace 0x100e210c), TPAdvisor.cpp
// for DMCreateMesh 0x100301c0 and the two DMEnableMeshRender 0x1003088c and 0x10030928, rsse.cpp for CorrectDTexturePaths 0x100b8710 and
// LLCellSystem.cpp for LLCC_SystemGetMode 0x100914f4.
//
// A mesh is N squares of four vertices and two faces each, drawn cell by cell by DrawAll (untranslated, the next unit). DMCreateMesh lays the
// whole mesh image out in one block of the engine allocator in the mesh file's 32-bit layout (Ptr32 fields; the block comes from the arena);
// the CDynamicMesh itself, its texture set, entry and mesh set are native records that point into the block.

// The records the other units hold by value or allocate (src/RideInstance.cpp, include/ui_DrawObject.h) keep their host sizes.
static_assert(sizeof(tag_sDMeshSet) == 112 && sizeof(sDMeshEntry) == 56, "the host sizes of the mesh set and its entry");

// ---- LLCellSystem.cpp

// 0x100914f4 (12 bytes): the cell system's mode.
int32_t LLCC_SystemGetMode() { return gLLCC480C; }

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

// The map reset (main app of SimTheme Park 2.04b3; the inventory files LLCC_ResetMap under LLCellSystem.cpp, HFieldCellStatus under
// TPRide.cpp, RemoveNeighbour and ModifyDirection under LLCellMouse.cpp and the cell resets under unrelated names; unit w112c):
// LLCC_ResetMap puts every cell of the map and of the track layer back to its start state (the cell's place comes from its id, the
// start state from the height field and, for the map cells, the attribute map).

// 0x100916a8 (204 bytes): rows first, a map cell then a track cell for each place. The place is the 16 bit number x + (y << 7) + 1
// (the same as the cell's id), so the cells are indexed by that number less one, in 32 bit arithmetic: number 0, which a 16 bit wrap
// gives, is the cell before the first. gGameWorld is read again before each call. The game passes (0x80, 0x80).
void LLCC_ResetMap(int32_t width, int32_t height) {
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const uint16_t place = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
            (gGameWorld->mMapCells + (place - 1))->Reset(true);
            (gGameWorld->mTrackCells + (place - 1))->Reset();
        }
    }
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

// The cell costs' setters (LLCellSystem.cpp range, main app, SimTheme Park 2.04b3; unit w117): four 12 byte functions, each storing its
// argument in a word of the cell system. No clamp, no answer. CGame::InitLevel_Initialise calls them with the level's balance.

int32_t gLLCC47DC = 0;   // 0x102e47dc  the path cost
int32_t gLLCC47D8 = 0;   // 0x102e47d8  the queue cost
int32_t gLLCC47D4 = 0;   // 0x102e47d4  the kart track cost
int32_t gLLCC47D0 = 0;   // 0x102e47d0  the water track cost

void LLCC_SystemSetPathCost(int32_t cost) { gLLCC47DC = cost; }          // 0x10091b24

// 0x10091b60, 0x10091bd8, 0x10091c54, 0x10091cd4 (12 bytes each): the four cost words (set by src/LLCellSystem_costs.cpp).
int32_t LLCC_SystemGetPathCost() { return gLLCC47DC; }

void LLCC_SystemSetQueueCost(int32_t cost) { gLLCC47D8 = cost; }         // 0x10091b9c

int32_t LLCC_SystemGetQueueCost() { return gLLCC47D8; }

void LLCC_SystemSetKartTrackCost(int32_t cost) { gLLCC47D4 = cost; }     // 0x10091c14

uint32_t LLCC_SystemGetKartTrackCost() { return static_cast<uint32_t>(gLLCC47D4); }

void LLCC_SystemSetWaterTrackCost(int32_t cost) { gLLCC47D0 = cost; }    // 0x10091c94

uint32_t LLCC_SystemGetWaterTrackCost() { return static_cast<uint32_t>(gLLCC47D0); }

// 0x10091d14 (12 bytes), 0x10091d50 (20 bytes, wraps), 0x10091d98 (12 bytes).
void LLCC_SystemSetSessionCost(int32_t cost) { gLLCC47CC = cost; }
void LLCC_SystemAddToSessionCost(int32_t cost) { gLLCC47CC = static_cast<int32_t>(static_cast<uint32_t>(gLLCC47CC) + static_cast<uint32_t>(cost)); }
int32_t LLCC_SystemGetSessionCost() { return gLLCC47CC; }

// The cell system's click stack and buy land list resets (main app, LLCellSystem.cpp range; unit w112a). Kept out of
// LLCellSystem.cpp, which unit w111's test builds with fakes of these names.

int32_t gLLCCClickStackCount;

// 0x10091dd4 (16 bytes): empties the click stack (its entry count is the one word the other click stack functions share).
void LLCC_SystemResetClickStack() { gLLCCClickStackCount = 0; }

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

// The click stacks of the queue and track tools (main app, LLCellSystem.cpp range 0x10092280 to 0x100936a8, SimTheme Park 2.04b3; unit w204):
// LLCC_SystemBacktrack 0x10092280, LLCC_SystemConstructQueueClickStack 0x10092644, LLCC_SystemConstructTrackClickStack 0x10092e44 and
// LLCC_SystemConstructReverseTrackClickStack 0x10093280. The stores and calls keep the listings' order: the logs compare them.
//
// Habits of the original that are kept:
//  - Every push on the stack is inlined by the compiler with LLCC_SystemAddToClickStack's rule (a count ABOVE 0x400 refuses, the entry 0x400 is
//    written one past the array: x lands in the count word and y in the word after it, whose high byte is gLLCC47C4; the count becomes x + 1),
//    except that the count is read again before every store and no answer is made. Written once here as PushClick.
//  - A cell is found by plain arithmetic on its 1 based id, (x + (y << 7) + 1) cut to 16 bits, with no test of x and y: the walks read cells
//    outside the grid when the cells they follow lead there (the original never tests the position before it reads a cell).
//  - The queue walk and the two track walks stop only on their tests (a direction that is none of the four compass bits never moves the position).
//  - The track walks step two cells at a time (the track cells lie on every second cell); the forward walk's stop at a start block of its own
//    parent MOVES two more cells, the reverse walk's does not.
//  - The forward track walk counts its passes in a register nobody reads, the reverse one too; both counters are left out. Backtrack tests its
//    pair of clicks for NULL twice after popping (the second test is dead).

// The click Backtrack pops (the Mac keeps it eight bytes before the copy of the stack; the name is invented).
LLCCClick gLLCCBacktrackClick = {0, 0};   // 0x102e07b0

namespace {

// The map cell and the track cell of a 1 based id: plain arithmetic on the world, as the Mac does it (id 0 lands one cell before the first).
CMapCell* MapCellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}

// The id of the cell at x and y, in 32 bits cut to 16 (the walks' positions are plain words).
uint16_t IdOfCell(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

// One push on the click stack (see the head of the file).
void PushClick(int32_t x, int32_t y) {
    const int32_t count = gLLCCClickStackCount;
    if (count > 0x400) return;
    if (count == 0x400) {
        gLLCCClickStackCount = x;
        gLLCC47C4 = static_cast<uint8_t>(static_cast<uint32_t>(y) >> 24);
    } else {
        gLLCCClickStack[count].mX = x;
        gLLCCClickStack[count].mY = y;
    }
    gLLCCClickStackCount = gLLCCClickStackCount + 1;
}

// The ride the cell system holds (gLLCCDB6C is a 32 bit pointer word on the Mac).
CObject* HeldRide() {
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}

// Backtrack's two ways out after a mode that sets the first block: with the flag and the bridge fixes. The held ride is read again before each call.
void FixBridgesOfHeldRide(bool fixBridges) {
    if (!fixBridges) return;
    if (gLLCCDB6C == 0) return;
    if (LLCR_FixBridges(HeldRide(), false, false, false) != 0) return;
    LLCR_FixBridgesReverse(HeldRide(), true, false, false);
}

// One step of the queue walk along a compass bit, one cell (0x10 down the y axis, 4 along x ...).
void StepOne(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 1; break;
        case 4: *x += 1; break;
        case 1: *y -= 1; break;
        case 0x40: *x -= 1; break;
        default: break;
    }
}

// The same step backwards.
void StepBack(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y -= 1; break;
        case 4: *x -= 1; break;
        case 1: *y += 1; break;
        case 0x40: *x += 1; break;
        default: break;
    }
}

// The track walks' step, two cells.
void StepTwo(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 2; break;
        case 4: *x += 2; break;
        case 1: *y -= 2; break;
        case 0x40: *x -= 2; break;
        default: break;
    }
}

// What both track walks do with a cell that holds track: a corner turns the direction (the opposite of the one it came with, xor the cell's
// neighbours) and is pushed, a crossroad or any other cell only sets the overlap counter. IsStraight is asked and its answer thrown away.
void TrackCellShape(CTrackCell* cell, uint8_t* direction, int32_t x, int32_t y) {
    if (cell->IsCorner() != 0) {
        const uint8_t opposite = OPPOSITE(*direction);
        const uint8_t neighbours = cell->GetNeighbours();
        *direction = static_cast<uint8_t>(opposite ^ neighbours);
        PushClick(x, y);
        cell->SetOverlapCounter(1);
    } else if (cell->IsCrossRoad() != 0) {
        cell->SetOverlapCounter(1);
    } else {
        cell->IsStraight();
        cell->SetOverlapCounter(0);
    }
}

}  // namespace

// 0x10092280, 912 bytes. The mode is read once. Mode 0x15 deletes the last brace; the modes 0xe, 0x13, 4, 0x3b and 0x16 do nothing. Otherwise the
// last click is popped from the stack: the click at the bottom (index 0) is NOT taken off (the count is put back) and counts as "no click".
// Answers 1 when a click was popped, else 0.
int32_t LLCC_SystemBacktrack(CObject* obj, bool fixBridges) {
    bool firstBlockFlag = false;
    bool queued = false;
    const int32_t mode = gLLCC480C;
    if (mode == 0x15) {
        DeleteLastTrackBrace();
        return 0;
    }
    if (mode == 0xe || mode == 0x13 || mode == 4 || mode == 0x3b || mode == 0x16) return 0;

    LLCCClick* click = nullptr;
    const int32_t count = gLLCCClickStackCount;
    if (count > 0) {
        gLLCCClickStackCount = count - 1;
        const int32_t index = gLLCCClickStackCount;
        gLLCCBacktrackClick.mX = gLLCCClickStack[index].mX;
        gLLCCBacktrackClick.mY = gLLCCClickStack[index].mY;
        if (index != 0) {
            click = &gLLCCBacktrackClick;
        } else {
            gLLCCClickStackCount = gLLCCClickStackCount + 1;
        }
    }

    if (click == nullptr) {
        if (mode == 0xb || mode == 0x10 || mode == 0x35) {
            LLCC_FixFirstBlock(firstBlockFlag);
            FixBridgesOfHeldRide(fixBridges);
        }
        LLCC_SystemSetModeNR(mode, true);
        return 0;
    }

    LLCM_SetIllegalPlacement(0);
    if (mode == 1 || mode == 3 || mode == 0x34) {
        LLCC_SystemSetMode(0x34, true);
        queued = true;
    } else if (mode == 0xb || mode == 0x10 || mode == 0x19 || mode == 0x35) {
        LLCC_SystemSetMode(0x35, true);
        gLLCCE288 = 0x80;
        gLLCCE28C = 0x80;
    }
    LLCM_LeftMouseButtonUp(click->mX, click->mY, 0, 0);

    // The stack may have grown in the call; its last click now becomes the pair (none when it is empty).
    const int32_t remaining = gLLCCClickStackCount;
    if (remaining > 0) {
        gLLCCBacktrackClick.mX = gLLCCClickStack[remaining - 1].mX;
        gLLCCBacktrackClick.mY = gLLCCClickStack[remaining - 1].mY;
    } else {
        click = nullptr;
    }
    if (remaining == 1) firstBlockFlag = true;
    if (click != nullptr) {
        // Mode 3 skips the second mouse up when the click's map cell is a path (mType 1).
        const CMapCell* cell = nullptr;
        if (mode == 3) cell = MapCellOfId(IdOfCell(click->mX, click->mY));
        if (mode != 3 || cell->mType != 1) {
            LLCM_LeftMouseButtonUp(click->mX, click->mY, 0, 0);
        }
        gLLCC4804 = click->mX;
        gLLCC4800 = click->mY;
    }
    LLCC_SystemSetModeNR(mode, true);
    if (queued) {
        if (obj != nullptr) {
            obj->RecalculateBackOfQueue();
        } else if (gLLCCDB6C != 0) {
            HeldRide()->RecalculateBackOfQueue();
        }
    }
    if (mode == 0xb || mode == 0x10 || mode == 0x35) {
        LLCC_FixFirstBlock(firstBlockFlag);
        FixBridgesOfHeldRide(fixBridges);
    }
    return 1;
}

// 0x10092644, 1980 bytes. Starts on the ride's entry cell (the halfword at +0x32) and follows the queue from there, pushing the corners; answers 0, or
// the direction byte when the walk met a cell that is not a queue. The walk's state: x and y (words), the direction, the answer and the stop flag.
int32_t LLCC_SystemConstructQueueClickStack(CObject* obj) {
    bool stop = false;
    int32_t answer = 0;
    bool moved = false;

    const uint16_t startId = obj->mF32;
    int32_t x = static_cast<int32_t>(startId) - 1;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(x) >> 7);
    x = x & 0x7f;
    gLLCCClickStackCount = 0;
    CMapCell* const start = MapCellOfId(IdOfCell(x, y));
    uint8_t direction = start->GetNeighbours();

    // A start cell with one neighbour looks at it first: a cell that is not a queue, or a queue of the same parent, is pushed; a queue of another
    // parent is where the walk begins (and ends: it goes straight to the last push).
    const MapDelta* source = nullptr;
    int32_t nx = x;
    int32_t ny = y;
    switch (direction) {
        case 0x10: source = &gMapDeltaPlusY; ny = y + 1; break;
        case 4: source = &gMapDeltaPlusX; nx = x + 1; break;
        case 1: source = &gMapDeltaMinusY; ny = y - 1; break;
        case 0x40: source = &gMapDeltaMinusX; nx = x - 1; break;
        default: break;
    }
    if (source != nullptr) {
        MapDelta delta = *source;
        CMapCell* const next = start->GetNeighbouringCell(&delta, 1);
        if (next != nullptr) {
            if (next->IsQueue() == 0) {
                PushClick(nx, ny);
            } else if (next->mParentID == start->mParentID) {
                PushClick(nx, ny);
            } else {
                moved = true;
                x = nx;
                y = ny;
            }
        }
    }

    if (!moved) {
        if (direction == 0 || !CMap::IsXYValid(x, y)) {
            // No neighbour (or the cell is outside the level): one step from the entry cell along its direction turned by the ride's rotation.
            const int32_t rotation = obj->mRotation;
            const uint8_t cellDirection = MapCellOfId(startId)->GetDirection();
            uint8_t turn = 0;
            switch (rotation) {
                case 0: turn = 1; break;
                case 0x5a: turn = 4; break;
                case 0xb4: turn = 0x10; break;
                case 0x10e: turn = 0x40; break;
                default: break;
            }
            StepOne(ADD_DIRECTION(cellDirection, turn), &x, &y);
        } else {
            do {
                CMapCell* const cell = MapCellOfId(IdOfCell(x, y));
                if (cell->IsQueue() == 0) {
                    stop = true;
                    answer = direction;
                } else if (cell->IsCorner() != 0) {
                    const uint8_t opposite = OPPOSITE(direction);
                    const uint8_t neighbours = cell->GetNeighbours();
                    direction = static_cast<uint8_t>(opposite ^ neighbours);
                    PushClick(x, y);
                    cell->SetOverlapCounter(1);
                } else {
                    cell->SetOverlapCounter(0);
                }
                if (cell->IsEntrance() == 0) {
                    if (cell->NumberOfNeighbours() == 1) stop = true;
                    if (cell->IsPath() != 0) {
                        // A path where the queue ends: the link between the cell and the one behind it is cut (unless that is an entrance, which
                        // keeps the walk on the path cell).
                        const int32_t savedX = x;
                        const int32_t savedY = y;
                        StepBack(direction, &x, &y);
                        CMapCell* const behind = MapCellOfId(IdOfCell(x, y));
                        if (behind->IsEntrance() != 0) {
                            x = savedX;
                            y = savedY;
                        } else {
                            behind->RemoveNeighbour(direction);
                            behind->SetHFieldTile();
                        }
                        cell->RemoveNeighbour(OPPOSITE(direction));
                        cell->SetHFieldTile();
                        stop = true;
                    }
                }
                if (!stop) StepOne(direction, &x, &y);
            } while (!stop);
        }
    }
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
    return answer;
}

// 0x10092e44, 1016 bytes. The track walked from the ride's start towards its end, two cells at a time (the track cells lie on every second cell).
void LLCC_SystemConstructTrackClickStack(CObject* obj) {
    bool stop = false;
    const uint16_t objectId = IdOfCell(obj->mBase.mCellX, obj->mBase.mCellY);
    int32_t x = (static_cast<int32_t>(objectId) - 1) & 0x7f;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(objectId) - 1) >> 7);
    uint8_t direction = LLCR_OffsetStartOfTrack(&x, &y, obj->mRotation);
    gLLCCE0CC = direction;
    gLLCCClickStackCount = 0;
    if (TrackCellOfId(IdOfCell(x, y))->IsJustTrack() != 0) PushClick(x, y);
    do {
        CTrackCell* const cell = TrackCellOfId(IdOfCell(x, y));
        if (cell->IsJustTrack() != 0) {
            TrackCellShape(cell, &direction, x, y);
            if (cell->IsCorner() == 0 && (cell->GetNeighbours() & direction) == 0) stop = true;
            CTrackCell* const next = cell->GetNeighbouringCell(direction, 2);
            if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
                const uint8_t back = OPPOSITE(direction);
                if ((back & next->GetNeighbours()) != 0) {
                    stop = true;
                    StepTwo(direction, &x, &y);
                }
            }
        }
        if (!stop) StepTwo(direction, &x, &y);
    } while (!stop);
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
}

// 0x10093280, 1064 bytes. The same walk from the other end of the track (LLCR_OffsetEndOfTrack) back towards the start. The first block's cell
// (gLLCCFirstBlockX and Y, when they are inside the level) gets its overlap counter cleared when the walk passes it; the walk also stops when its
// own first cell is not track at all. A start block of the cell's own parent stops the walk where it stands.
void LLCC_SystemConstructReverseTrackClickStack(CObject* obj) {
    bool stop = false;
    uint16_t firstBlockId = 0;
    const uint16_t objectId = IdOfCell(obj->mBase.mCellX, obj->mBase.mCellY);
    int32_t x = (static_cast<int32_t>(objectId) - 1) & 0x7f;
    int32_t y = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(objectId) - 1) >> 7);
    uint8_t direction = LLCR_OffsetEndOfTrack(&x, &y, obj->mRotation);
    gLLCCE0CC = direction;
    if (CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY) != 0) {
        const int32_t firstY = gLLCCFirstBlockY;
        const int32_t firstX = gLLCCFirstBlockX;
        firstBlockId = IdOfCell(firstX, firstY);
    }
    gLLCCClickStackCount = 0;
    CTrackCell* const startCell = TrackCellOfId(IdOfCell(x, y));
    if (startCell->IsJustTrack() != 0) PushClick(x, y);
    if (startCell->IsTrack() == 0) stop = true;
    do {
        CTrackCell* const cell = TrackCellOfId(IdOfCell(x, y));
        if (cell->IsJustTrack() != 0) {
            TrackCellShape(cell, &direction, x, y);
            if (cell->mCellId == firstBlockId) cell->SetOverlapCounter(0);
            if (cell->IsCorner() == 0 && (cell->GetNeighbours() & direction) == 0) stop = true;
            CTrackCell* const next = cell->GetNeighbouringCell(direction, 2);
            if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
                const uint8_t back = OPPOSITE(direction);
                if ((back & next->GetNeighbours()) != 0) stop = true;
            }
        }
        if (!stop) StepTwo(direction, &x, &y);
    } while (!stop);
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
}

// The park's hoarding in LLCellSystem.cpp of the main app (SimTheme Park 2.04b3; unit w201): the walk along it (the click stack, the lock flag), the
// default and balance file hoardings, the revision of its corners, and the image of the park (current and new, a 0x43 by 0x43 grid of half cells)
// with the flood fill and the legality checks that read it. The stores and calls keep the listing's order: the logs compare them.
//
// The names of the words are invented (the header says so). The two images are one array: the Mac keeps the new image (0x102de198) and the current
// one (0x102df321) back to back, so an area handed to LLCC_AddNewAreaToImage that reaches past the new image writes into the current one.

int32_t gLLCCReviseAnswer = 1;                           // 0x1029df30
uint8_t gLLCCHoardingImages[2 * 0x43 * 0x43];            // 0x102de198 (new), 0x102df321 (current)

namespace {

constexpr int32_t kImageSide = 0x43;
constexpr int32_t kImageSize = kImageSide * kImageSide;   // 0x1189

uint8_t* NewImage() { return gLLCCHoardingImages; }
uint8_t* CurrentImage() { return gLLCCHoardingImages + kImageSize; }

// The cell id of (x, y): 1 + x + (y << 7), 16 bits (the shift is the 32-bit one of the Mac).
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + ((static_cast<uint32_t>(y) << 7) + 1u));
}

// The pieces of the walk along the hoarding that the two walkers share (the listings repeat them): the heading after a corner (the click stack's walker
// keeps the push between the heading and the stop tests, so it writes the corner out itself), the stop tests (`steps` is the number of steps made so far)
// and the move of two cells.
void TurnAtCorner(CTrackCell* cell, uint8_t* direction) {
    if (cell->mType == 0x19 && cell->IsCorner() != 0) {
        *direction = OPPOSITE(*direction);
        *direction = static_cast<uint8_t>(*direction ^ cell->GetNeighbours());
    }
}

void StopTests(CTrackCell* cell, uint8_t direction, int32_t steps, bool* stop) {
    CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
    if (next != nullptr && next->mType != 0x19) *stop = true;
    if (cell->NumberOfNeighbours() == 1 && steps != 0) *stop = true;
}

void Move(uint8_t direction, int32_t* x, int32_t* y) {
    switch (direction) {
        case 0x10: *y += 2; break;
        case 4: *x += 2; break;
        case 1: *y -= 2; break;
        case 0x40: *x -= 2; break;
        default: break;
    }
}

}  // namespace

// 0x100936f4, 512 bytes. Starts at (right, bottom) heading 4, pushes the start, then walks the hoarding two cells at a time: at a corner the heading
// becomes the opposite of the heading XOR the cell's neighbours (and the cell is pushed), the walk stops when the next cell is not hoarding, or when
// the cell has one neighbour and this is not the first step. The end is pushed too and left in gLLCC4804 (x) and gLLCC4800 (y). The walk has no step limit.
void LLCC_SystemConstructHoardingClickStack() {
    uint8_t direction = 4;
    bool stop = false;
    int32_t steps = 0;
    int32_t x = gHoardingRight;
    int32_t y = gHoardingBottom;
    gLLCCE0CC = 4;
    gLLCCClickStackCount = 0;
    gLLCCClickStack[0].mX = x;
    gLLCCClickStack[0].mY = y;
    gLLCCClickStackCount = gLLCCClickStackCount + 1;
    do {
        CTrackCell* cell = TrackCellOfId(CellId(x, y));
        if (cell->mType == 0x19 && cell->IsCorner() != 0) {
            direction = OPPOSITE(direction);
            direction = static_cast<uint8_t>(direction ^ cell->GetNeighbours());
            PushClick(x, y);
        }
        StopTests(cell, direction, steps, &stop);
        if (!stop) Move(direction, &x, &y);
        ++steps;
    } while (!stop);
    PushClick(x, y);
    gLLCC4804 = x;
    gLLCC4800 = y;
}

// 0x10093934, 432 bytes. The same walk without the stack: every cell on the way gets the lock flag 0x20 set (lock) or cleared, BEFORE its corner test.
void LLCC_LockHoarding(bool lock) {
    uint8_t direction = 4;
    bool stop = false;
    int32_t steps = 0;
    int32_t x = gHoardingRight;
    int32_t y = gHoardingBottom;
    gLLCCE0CC = 4;
    do {
        CTrackCell* cell = TrackCellOfId(CellId(x, y));
        if (lock) {
            cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() | 0x20));
        } else {
            cell->UnSetFlag(0x20);
        }
        TurnAtCorner(cell, &direction);
        StopTests(cell, direction, steps, &stop);
        if (!stop) Move(direction, &x, &y);
        ++steps;
    } while (!stop);
}

// The park's inside and the hoarding edit check in LLCellSystem.cpp of the main app (SimTheme Park 2.04b3; unit w206): LLCC_FillInsideOfPark
// (0x10093b0c, 2,836 bytes) and LLCC_IsHoardingEditLegal (0x1009464c, 3,488 bytes). The calls keep the listing's order: the logs compare them.
//
// The map flag 0x40 marks the land outside the park (inferred: the cost loop of the edit check charges every such cell of the new area, and the
// fill below clears it from the cells inside the hoarding). Track cells flagged 0x20 are the locked hoarding (LLCC_LockHoarding).
//
// The names of the eight words are invented (the header says so). Cell arithmetic is the Mac's plain 16 bit id arithmetic on the world block, so the
// scans of the fill (west, south and east) have no bound of their own: they end at a locked cell, and a hoarding that is not locked walks them out of
// the world.

int32_t gLLCCEdgeCount[4];         // 0x102e05a0
uint8_t gLLCCEdgeMarks[4][0x80];   // 0x102e05b0

namespace {

// The balance row after WaterTrackCell (Costs.MapCell, Mac +0x5cc in src/MainBalance.cpp): the price of one cell of land.
constexpr int kMapCellCostSlot = 369;

// A map cell and its three neighbours towards +y, +x +y and +x (asked in that order, each with a fresh copy of the compass offset). A
// neighbour outside the level is NULL and the Mac reads through it.
struct Square {
    CMapCell* mCell[4];
};

Square SquareAt(CMapCell* map) {
    Square square;
    square.mCell[0] = map;
    MapDelta d1 = gMapDeltaPlusY;
    square.mCell[1] = map->GetNeighbouringCell(&d1, 1);
    MapDelta d2 = gMapDeltaPlusXPlusY;
    square.mCell[2] = map->GetNeighbouringCell(&d2, 1);
    MapDelta d3 = gMapDeltaPlusX;
    square.mCell[3] = map->GetNeighbouringCell(&d3, 1);
    return square;
}

// Whether one of the four cells has the flag 0x40 (the Mac stops asking at the first that has).
bool AnyFlagged(const Square& square) {
    if (square.mCell[0]->IsFlag(0x40) != 0) return true;
    if (square.mCell[1]->IsFlag(0x40) != 0) return true;
    if (square.mCell[2]->IsFlag(0x40) != 0) return true;
    return square.mCell[3]->IsFlag(0x40) != 0;
}

// Takes the flag 0x40 off the four cells and re-tiles each one, cell by cell.
void ClearAndRetile(const Square& square) {
    square.mCell[0]->UnSetFlag(0x40);
    square.mCell[0]->SetHFieldTile();
    square.mCell[1]->UnSetFlag(0x40);
    square.mCell[1]->SetHFieldTile();
    square.mCell[2]->UnSetFlag(0x40);
    square.mCell[2]->SetHFieldTile();
    square.mCell[3]->UnSetFlag(0x40);
    square.mCell[3]->SetHFieldTile();
}

// A cell the inward scan has reached: when it is not locked, its map cell and the three after it lose the flag 0x40 (if any of them has it).
void ClearInside(CTrackCell* scanned) {
    if (scanned->IsFlag(0x20) != 0) return;
    CMapCell* map = scanned->GetMapCellEquiv();
    const Square square = SquareAt(map);
    if (AnyFlagged(square)) ClearAndRetile(square);
}

// The four scans across the park, one for each heading of the walk (the heading is the direction of the walk along the hoarding): the cells at
// two cell steps from the walk's cell up to and including the first locked one. Heading 0x10 scans towards -x, 4 towards +y, 1 towards +x.
void ScanTowardsMinusX(int32_t x, int32_t y) {
    int32_t k = x;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(k - 2, y));
        ClearInside(scanned);
        locked = scanned->IsFlag(0x20);
        k -= 2;
    } while (locked == 0);
}

void ScanTowardsPlusY(int32_t x, int32_t y) {
    int32_t k = y;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(x, k + 2));
        ClearInside(scanned);
        locked = scanned->IsFlag(0x20);
        k += 2;
    } while (locked == 0);
}

void ScanTowardsPlusX(int32_t x, int32_t y) {
    int32_t k = x;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(k + 2, y));
        ClearInside(scanned);
        locked = scanned->IsFlag(0x20);
        k += 2;
    } while (locked == 0);
}

// Heading 0x40 scans towards -y from the cell two rows up; it also stops (after the cell that made the row negative) at the top of the level, and
// `reachedTop` stays set for the rest of the fill, so a later scan of this kind scans one cell only.
void ScanTowardsMinusY(int32_t x, int32_t y, bool* reachedTop) {
    int32_t k = y - 2;
    int32_t locked;
    do {
        CTrackCell* scanned = TrackCellOfId(CellId(x, k));
        ClearInside(scanned);
        k -= 2;
        if (k < 0) *reachedTop = true;
        locked = scanned->IsFlag(0x20);
    } while (locked == 0 && !*reachedTop);
}

// The sides of the edited rectangle, in the order of the tally words (gLLCCEdgeCount, gLLCCEdgeMarks): the column at the larger x, the row at the
// larger y, the column at the smaller x and the row at the smaller y.
enum Side { kHixColumn = 0, kHiyRow = 1, kLoxColumn = 2, kLoyRow = 3 };

// What the check counts along one side: the run of freeform dry cells met in a row, the hoarding cells, and of those the straight ones with the
// neighbour byte of the side's direction and all the others.
struct SideTally {
    int32_t mRun;
    int32_t mHoarding;
    int32_t mStraight;
    int32_t mOther;
};

// One cell on a side of the rectangle. Two freeform dry cells in a row make the edit illegal (`bad`); the mark says whether the cell lies inside the
// park (no flag 0x40) or is hoarding; `atEnd` is a cell at either end of the side (a corner when it is hoarding); `straightBytes` is 0x11 on a row
// and 0x44 on a column.
void TallySideCell(CTrackCell* cell, Side side, SideTally* tally, bool atEnd, uint8_t straightBytes, int32_t* corners, bool* bad) {
    CMapCell* map = cell->GetMapCellEquiv();
    if (map->IsFreeform() == 0 || map->IsWater() != 0) {
        tally->mRun = 0;
    } else {
        tally->mRun = tally->mRun + 1;
        if (tally->mRun > 1) {
            tally->mRun = 0;
            *bad = true;
        }
    }
    if (cell->IsHoarding() != 0) tally->mHoarding = tally->mHoarding + 1;
    uint8_t mark = 0;
    if (map->IsFlag(0x40) == 0 || cell->IsHoarding() != 0) mark = 1;
    const int32_t slot = gLLCCEdgeCount[side];
    gLLCCEdgeCount[side] = slot + 1;
    gLLCCEdgeMarks[side][slot] = mark;
    if (cell->IsHoarding() != 0) {
        if (atEnd) *corners = *corners + 1;
        if (cell->IsStraight() == 0 || cell->GetNeighbours() != straightBytes) {
            tally->mOther = tally->mOther + 1;
        } else {
            tally->mStraight = tally->mStraight + 1;
        }
    }
}

}  // namespace

// 0x10093b0c, 2,836 bytes. Walks the locked hoarding as LLCC_LockHoarding does (from (right, bottom), heading 4, two cells a step; the stop and
// the corner turn are the same) and at every hoarding cell on the walk scans across the park to the next locked cell: every unlocked cell the scan
// passes gives up the flag 0x40 (with its +y, +x +y and +x neighbours) and has its tile set again. Then every hoarding cell left in the level that is
// not locked (a hoarding cell behind a track cell is found through the parent) is deleted, its overlap counter cleared and its map cell and the three
// after it freed the same way.
void LLCC_FillInsideOfPark() {
    uint8_t direction = 4;
    bool stop = false;
    int32_t steps = 0;
    bool reachedTop = false;
    int32_t x = gHoardingRight;
    int32_t y = gHoardingBottom;
    gLLCCE0CC = 4;
    do {
        CTrackCell* cell = TrackCellOfId(CellId(x, y));
        if (cell->mType == 0x19) {
            switch (direction) {
                case 0x10: ScanTowardsMinusX(x, y); break;
                case 4: ScanTowardsPlusY(x, y); break;
                case 1: ScanTowardsPlusX(x, y); break;
                case 0x40: ScanTowardsMinusY(x, y, &reachedTop); break;
                default: break;
            }
            if (cell->IsCorner() != 0) {
                direction = OPPOSITE(direction);
                direction = static_cast<uint8_t>(direction ^ cell->GetNeighbours());
            }
        }
        CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
        if (next != nullptr && next->mType != 0x19) stop = true;
        if (cell->NumberOfNeighbours() == 1 && steps != 0) stop = true;
        if (!stop) Move(direction, &x, &y);
        ++steps;
    } while (!stop);
    for (int32_t row = 0; row < gLLCCHeight - 1; ++row) {
        for (int32_t column = 0; column < gLLCCWidth - 1; ++column) {
            CTrackCell* cell = TrackCellOfId(CellId(column, row));
            if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
            if (cell->IsHoarding() != 0 && cell->IsFlag(0x20) == 0) {
                CMapCell* map = cell->GetMapCellEquiv();
                const Square square = SquareAt(map);
                if (map->mType == 0x15) {
                    cell->QuickDelete();
                } else {
                    cell->Delete(0, 0, false);
                }
                cell->SetOverlapCounter(0);
                ClearAndRetile(square);
            }
        }
    }
}

// 0x1009464c, 3,488 bytes. Whether the rectangle from (x1, y1) to (x2, y2) (corners in any order) may become the park's hoarding: 1, or 0 for a
// corner outside the level, a rectangle two cells wide or less, an edge loop that visits (right, bottom) or (left, top) of the hoarding's extents on
// a side of the rectangle, two freeform dry cells in a row along a side, two hoarding corners and no straight hoarding piece on any side (after the
// fix below), an area that is all inside or all outside the park with fewer than two hoarding cells on each side, a refusal of LLCC_SoIsItLegal
// (its low byte) and, when `charge` is set, land that costs more than the bank holds (gLLCCC204 is set to 1 then). The tallies are left in
// gLLCCEdgeCount and gLLCCEdgeMarks (0x80 bytes a side, the unused marks 0xff).
int32_t LLCC_IsHoardingEditLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool charge) {
    const int32_t lox = x2 < x1 ? x2 : x1;
    const int32_t loy = y2 < y1 ? y2 : y1;
    const int32_t hix = x1 < x2 ? x2 : x1;
    const int32_t hiy = y1 < y2 ? y2 : y1;
    gLLCCEdgeCount[kLoyRow] = 0;
    gLLCCEdgeCount[kLoxColumn] = 0;
    gLLCCEdgeCount[kHiyRow] = 0;
    gLLCCEdgeCount[kHixColumn] = 0;
    bool bad = false;
    bool broke = false;
    for (int32_t i = 0; i < 0x80; ++i) {
        gLLCCEdgeMarks[kLoyRow][i] = 0xff;
        gLLCCEdgeMarks[kLoxColumn][i] = 0xff;
        gLLCCEdgeMarks[kHiyRow][i] = 0xff;
        gLLCCEdgeMarks[kHixColumn][i] = 0xff;
    }
    gLLCC47CC = 0;
    if (!CMap::IsXYValid(lox, loy)) return 0;
    if (!CMap::IsXYValid(hix, hiy)) return 0;
    if (hix - lox <= 2 || hiy - loy <= 2) return 0;
    SideTally sides[4] = {};
    int32_t corners = 0;
    // The cell of the first corner is asked for its flag; the answer is not used.
    MapCellOfId(CellId(x1, y1))->IsFlag(0x40);
    for (int32_t x = lox; x <= hix; x += 2) {
        if (x == gHoardingRight && (loy == gHoardingBottom || hiy == gHoardingBottom)) return 0;
        if (x == gHoardingLeft && (loy == gHoardingTop || hiy == gHoardingTop)) return 0;
        TallySideCell(TrackCellOfId(CellId(x, loy)), kLoyRow, &sides[kLoyRow], x == lox || x == hix, 0x11, &corners, &bad);
        TallySideCell(TrackCellOfId(CellId(x, hiy)), kHiyRow, &sides[kHiyRow], x == lox || x == hix, 0x11, &corners, &bad);
    }
    for (int32_t y = loy; y <= hiy; y += 2) {
        if ((lox == gHoardingRight || hix == gHoardingRight) && y == gHoardingBottom) return 0;
        if ((lox == gHoardingLeft || hix == gHoardingLeft) && y == gHoardingTop) return 0;
        TallySideCell(TrackCellOfId(CellId(lox, y)), kLoxColumn, &sides[kLoxColumn], y == loy || y == hiy, 0x44, &corners, &bad);
        TallySideCell(TrackCellOfId(CellId(hix, y)), kHixColumn, &sides[kHixColumn], y == loy || y == hiy, 0x44, &corners, &bad);
    }
    // A side with more than one hoarding cell that is not straight gets one straight piece added, or, when the cell at the corner it starts from
    // is hoarding (the first point for the sides at loy and lox, the second point for the sides at hiy and hix), the opposite side loses one.
    CTrackCell* const first = TrackCellOfId(CellId(x1, y1));
    CTrackCell* const second = TrackCellOfId(CellId(x2, y2));
    if (sides[kLoyRow].mOther > 1) {
        if (first->mType == 0x19) {
            sides[kHiyRow].mStraight = sides[kHiyRow].mStraight - 1;
        } else {
            sides[kLoyRow].mStraight = sides[kLoyRow].mStraight + 1;
        }
    }
    if (sides[kLoxColumn].mOther > 1) {
        if (first->mType == 0x19) {
            sides[kHixColumn].mStraight = sides[kHixColumn].mStraight - 1;
        } else {
            sides[kLoxColumn].mStraight = sides[kLoxColumn].mStraight + 1;
        }
    }
    if (sides[kHiyRow].mOther > 1) {
        if (second->mType == 0x19) {
            sides[kLoyRow].mStraight = sides[kLoyRow].mStraight - 1;
        } else {
            sides[kHiyRow].mStraight = sides[kHiyRow].mStraight + 1;
        }
    }
    if (sides[kHixColumn].mOther > 1) {
        if (second->mType == 0x19) {
            sides[kLoxColumn].mStraight = sides[kLoxColumn].mStraight - 1;
        } else {
            sides[kHixColumn].mStraight = sides[kHixColumn].mStraight + 1;
        }
    }
    if (corners == 2 && sides[kLoyRow].mStraight == 0 && sides[kHiyRow].mStraight == 0 && sides[kLoxColumn].mStraight == 0 &&
        sides[kHixColumn].mStraight == 0) {
        return 0;
    }
    if (bad) return 0;
    int32_t inside = 0;
    int32_t outside = 0;
    for (int32_t y = loy; y <= hiy; ++y) {
        for (int32_t x = lox; x <= hix; ++x) {
            CTrackCell* cell = TrackCellOfId(CellId(x, y));
            CMapCell* map = cell->GetMapCellEquiv();
            if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
            if (map->IsFlag(0x40) == 0 || cell->IsHoarding() != 0) {
                ++inside;
            } else {
                ++outside;
            }
        }
    }
    if ((inside == 0 || outside == 0) && sides[kLoyRow].mHoarding < 2 && sides[kLoxColumn].mHoarding < 2 && sides[kHiyRow].mHoarding < 2 &&
        sides[kHixColumn].mHoarding < 2) {
        return 0;
    }
    LLCC_CopyCurrentToNewImage();
    LLCC_AddNewAreaToImage(lox, loy, hix, hiy);
    if (static_cast<uint8_t>(LLCC_SoIsItLegal()) == 0) return 0;
    if (charge) {
        for (int32_t y = loy + 2; y < hiy; ++y) {
            for (int32_t x = lox + 2; x < hix; ++x) {
                CMapCell* map = MapCellOfId(CellId(x, y));
                if (map->IsFlag(0x40) != 0) {
                    gLLCC47CC = static_cast<int32_t>(static_cast<uint32_t>(gLLCC47CC) + static_cast<uint32_t>(gMainBalance.mValues[kMapCellCostSlot].mInteger));
                    if (gActionRecorder.mRecording == 0) {
                        const int32_t cost = gLLCC47CC;
                        CBankAccount* account = gGameWorld->GetBankAccount();
                        const int32_t balance = account->GetBalance();
                        if (static_cast<int32_t>(static_cast<uint32_t>(balance) - static_cast<uint32_t>(cost)) < 0) broke = true;
                    } else {
                        broke = false;
                    }
                }
            }
        }
        if (broke) {
            gLLCCC204 = 1;
            return 0;
        }
    }
    return 1;
}

// LLCellSystem.cpp range of the main app (SimTheme Park 2.04b3), unit w133: LLCC_DrawInitialHoarding (0x10095420, 600 bytes).

int32_t gHoardingLeft = -1;     // 0x1029e044
int32_t gHoardingTop = -1;      // 0x1029e048
int32_t gHoardingRight = -1;    // 0x1029e03c
int32_t gHoardingBottom = -1;   // 0x1029e040
uint8_t gLLCCE0CC = 0;           // 0x102de0cc

// The park's first hoarding. The extents come from the park entrances' cell ids (1-based: row = (id - 1) >> 7 (a logical shift), column = (id - 1) & 0x7f):
// left = column - 4 and top = row of entrance A (0), right = column + 3 and bottom = row of entrance B (1), all four in the globals. The cell
// system's mode is set to 0x19 while the hoardings are drawn (from the balance file, or the default ones when dialog option 0x400
// is set), the inside of the park is filled between locking and unlocking, and the hoarding database is built from the extents. Then, with
// mode 0, the two rows of cells at bottom and bottom + 1 from column left + 2 up to right - 1 lose cell flag 0x40 and get their height
// field tile set (the cell index is 16-bit: ushort(x + row * 128 + 1) - 1). The flag byte gLLCC4818 is 1 meanwhile, and the park's
// darkening is recalculated at the end.
void LLCC_DrawInitialHoarding() {
    gLLCC4818 = 1;
    const uint16_t first = gGameWorld->GetMap()->GetParkEntrancePos(0).mIndex;
    const uint16_t second = gGameWorld->GetMap()->GetParkEntrancePos(1).mIndex;
    gHoardingLeft = ((static_cast<int32_t>(first) - 1) & 0x7f) - 4;
    gHoardingTop = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(first) - 1) >> 7);
    gHoardingRight = ((static_cast<int32_t>(second) - 1) & 0x7f) + 3;
    gHoardingBottom = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(second) - 1) >> 7);
    gLLCCE0CC = 4;
    LLCC_SystemSetMode(0x19, true);
    LLCM_SetCurrentObject(true);
    if ((gTPDialogOptions & 0x400) == 0) {
        LLCC_DrawHoardingsFromBalanceFile(&gHoardingsBalance434C);
    } else {
        LLCC_DrawDefaultHoardings();
    }
    LLCC_LockHoarding(true);
    LLCC_FillInsideOfPark();
    LLCC_LockHoarding(false);
    LLCC_BuildHoardingDB(gHoardingRight, gHoardingBottom, gHoardingLeft, gHoardingTop, 4, true);
    LLCC_SystemSetMode(0, true);
    for (int32_t x = gHoardingLeft + 2; x < gHoardingRight; ++x) {
        char* map = reinterpret_cast<char*>(gGameWorld->GetMap());
        const int32_t indexA = static_cast<int32_t>(static_cast<uint16_t>(x + static_cast<int32_t>(static_cast<uint32_t>(gHoardingBottom) << 7) + 1)) - 1;
        CMapCell* cell = reinterpret_cast<CMapCell*>(map + static_cast<ptrdiff_t>(indexA) * 0x44);
        cell->UnSetFlag(0x40);
        cell->SetHFieldTile();
        map = reinterpret_cast<char*>(gGameWorld->GetMap());
        const int32_t indexB = static_cast<int32_t>(static_cast<uint16_t>(x + (gHoardingBottom + 1) * 0x80 + 1)) - 1;
        cell = reinterpret_cast<CMapCell*>(map + static_cast<ptrdiff_t>(indexB) * 0x44);
        cell->UnSetFlag(0x40);
        cell->SetHFieldTile();
    }
    gLLCC4818 = 0;
    RecalculateParkDarkening();
}

// 0x100956a8, 204 bytes: the six clicks of the default hoarding, relative to (right, bottom) (read once).
void LLCC_DrawDefaultHoardings() {
    const int32_t x = gHoardingRight;
    const int32_t y = gHoardingBottom;
    LLCM_LeftMouseButtonUp(x, y, 0, 0);
    LLCM_LeftMouseButtonUp(x + 2, y, 0, 0);
    LLCM_LeftMouseButtonUp(x + 2, y + 4, 0, 0);
    LLCM_LeftMouseButtonUp(x - 10, y + 4, 0, 0);
    LLCM_LeftMouseButtonUp(x - 10, y, 0, 0);
    LLCM_LeftMouseButtonUp(x - 8, y, 0, 0);
}

// 0x100957a4, 156 bytes: the balance file's pairs of offsets (the table's values 1 and 2, then 3 and 4 ...) are clicked relative to (right, bottom) until
// an x offset of 0x200 or the 64th pair; the value is read BEFORE the count is tested (so at the 65th pair the Mac reads value 129). The two extents
// are read again for every click. The Mac leaves 1 in r3, which nobody reads.
void LLCC_DrawHoardingsFromBalanceFile(CHoardingsBalance* balance) {
    int32_t i = 0;
    for (;;) {
        if (balance->mValues[1 + 2 * i].mInteger == 0x200 || i > 0x3f) break;
        LLCM_LeftMouseButtonUp(gHoardingRight + balance->mValues[1 + 2 * i].mInteger, gHoardingBottom + balance->mValues[2 + 2 * i].mInteger, 0, 0);
        ++i;
    }
}

// Unit w208 (main app, SimTheme Park 2.04b3): the park's hoarding written to the theme's hoardings balance file, and the static objects of
// LLCellSystem.cpp.
//   LLCC_WriteHoardingsLine                   0x1009588c  580 bytes
//   LLCC_WriteHoardingsToBalanceFile          0x10095b20  764 bytes
//   LLCC_WriteCompleteHoardingsToBalanceFile  0x10095e54  764 bytes
//   __sinit_LLCellSystem_cpp                  0x10096f04  764 bytes (the definitions just below)

// __sinit_LLCellSystem_cpp (0x10096f04) builds these in this order, and registers a destructor for each object that has one with
// __register_global_object: two plain words, the file, the seven strings, then the nine offsets (stored as two words each, the destructor of the
// MapDelta statics registered for each). The host's definitions in the same order stand in for it; the words and the offsets need no construction.
int32_t gLLCC4820 = 0;                                    // 0x102e4820
int32_t gLLCC481C = 0x10000;                              // 0x102e481c
bfl::FileSystem::CDiscFile gLLCCHoardingsFile;            // 0x102e058c  (its destructor 0x103bcb68)
TbDynamicStringTemplate<char> gLLCCHoardingFileHeader("#ThemeParkWorld - Hoardings.sam");   // 0x102e056c
TbDynamicStringTemplate<char> gLLCCHoardingFileNote("#DO NOT EDIT BY HAND");                // 0x102e054c
TbDynamicStringTemplate<char> gLLCCHoardingKeyPrefix("HoardingClicks[");                    // 0x102e052c
TbDynamicStringTemplate<char> gLLCCHoardingKeyX("].X");                                     // 0x102e050c
TbDynamicStringTemplate<char> gLLCCHoardingKeyY("].Y");                                     // 0x102e04ec
TbDynamicStringTemplate<char> gLLCCHoardingEOL("\n");                                       // 0x102e04cc
TbDynamicStringTemplate<char> gLLCCHoardingSep("\t");                                       // 0x102e04ac
const MapDelta gLLCCPointedOffsets[9] = {
    {0, 0, 0},     // 0x102de0e4
    {-1, -1, 0},   // 0x102de0ec
    {0, -1, 0},    // 0x102de0f4
    {1, -1, 0},    // 0x102de0fc
    {-1, 0, 0},    // 0x102de104
    {1, 0, 0},     // 0x102de10c
    {-1, 1, 0},    // 0x102de114
    {0, 1, 0},     // 0x102de11c
    {1, 1, 0},     // 0x102de124
};

namespace {

// One write to the file: the length of the text, then the file's access object (a file that failed to open has none: nothing is written).
// The Mac inlines CFile::Write, which tests the access object each time.
void WriteText(const TbDynamicStringTemplate<char>& text) {
    const uint32_t length = static_cast<uint32_t>(std::strlen(text.mData));
    gLLCCHoardingsFile.Write(text.mData, length);
}

// The next click of the copied stack, which both writers inline (the 8 bytes at 0x102e07b0 are the click it hands out; w204's Backtrack pops
// the same one). The index and the count are compared signed: an index past the count ends the walk at once, otherwise the click at the index is
// copied out, the index moves on and, when it has gone past the count, goes back (the click then handed out is not used). The copy after the last
// click is read too: at the entry 0x400 the Mac reads the index word and the count word that follow the array. Counts above 0x400 are not modelled.
const LLCCClick* NextCopiedClick() {
    const int32_t index = gLLCCClickCopyIndex;
    const int32_t count = gLLCCClickCopyCount;
    if (index > count) return nullptr;
    if (index == 0x400) {
        gLLCCBacktrackClick.mX = index;
        gLLCCBacktrackClick.mY = count;
    } else {
        gLLCCBacktrackClick = gLLCCClickStackCopy[index];
    }
    gLLCCClickCopyIndex = gLLCCClickCopyIndex + 1;
    if (gLLCCClickCopyIndex > count) {
        gLLCCClickCopyIndex = gLLCCClickCopyIndex - 1;
        return nullptr;
    }
    return &gLLCCBacktrackClick;
}

// The two writers are the same code with a different file name (a member of gTPGlobals) and trace text: the Mac has two copies of it.
int32_t WriteBalanceFile(const TbDynamicStringTemplate<char>& name, const char* trace) {
    TbDynamicStringTemplate<char> path;
    int32_t i = 0;
    path = get_theme_path();
    FS_AddDirectorySeparator(path);
    path += name;
    CTRACE(6, trace);
    const TbError opened = gLLCCHoardingsFile.Open(path.mData, 0xC0000010);
    if (static_cast<int32_t>(opened) < 0) {
        return 0;
    }
    // The click stack is built with gLLCC4818 set; the copy of it is LLCC_SystemMakeClickStackCopy's loop, inlined.
    gLLCC4818 = 1;
    LLCC_SystemConstructHoardingClickStack();
    gLLCC4818 = 0;
    for (int32_t k = 0; k < gLLCCClickStackCount; ++k) {
        gLLCCClickStackCopy[k] = gLLCCClickStack[k];
    }
    gLLCCClickCopyCount = gLLCCClickStackCount;
    gLLCCClickCopyIndex = 0;
    for (const LLCCClick* click = NextCopiedClick(); click != nullptr; click = NextCopiedClick()) {
        const int32_t x = static_cast<int32_t>(static_cast<uint32_t>(click->mX) - static_cast<uint32_t>(gHoardingRight));
        if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyX, i, x) == 0) {
            return 0;
        }
        const int32_t y = static_cast<int32_t>(static_cast<uint32_t>(click->mY) - static_cast<uint32_t>(gHoardingBottom));
        if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyY, i, y) == 0) {
            return 0;
        }
        // A blank row between two clicks (the temporary string is destroyed before the answer is tested).
        if (LLCC_WriteHoardingsLine(TbDynamicStringTemplate<char>(""), -1, 0) == 0) {
            return 0;
        }
        ++i;
    }
    if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyX, i, 0x200) == 0) {
        return 0;
    }
    if (LLCC_WriteHoardingsLine(gLLCCHoardingKeyY, i, 0x200) == 0) {
        return 0;
    }
    gLLCCHoardingsFile.Close();
    return 1;
}

}  // namespace

// 0x1009588c (580 bytes). Every piece goes out through WriteText; the digits are made by the bullfrog glue _itoa into a stack buffer.
int32_t LLCC_WriteHoardingsLine(const TbDynamicStringTemplate<char>& key, int32_t index, int32_t value) {
    char buffer[40];
    if (index == -1) {
        WriteText(gLLCCHoardingEOL);
        return 1;
    }
    WriteText(gLLCCHoardingKeyPrefix);
    NS_MacDoze::_itoa(index, buffer, 10);
    gLLCCHoardingsFile.Write(buffer, static_cast<uint32_t>(std::strlen(buffer)));
    WriteText(key);
    WriteText(gLLCCHoardingSep);
    NS_MacDoze::_itoa(value, buffer, 10);
    gLLCCHoardingsFile.Write(buffer, static_cast<uint32_t>(std::strlen(buffer)));
    WriteText(gLLCCHoardingEOL);
    return 1;
}

// 0x10095b20 (764 bytes). Every failed row (never: the row writer always answers 1) returns 0 with the path destroyed and the file left open.
int32_t LLCC_WriteHoardingsToBalanceFile() {
    return WriteBalanceFile(gTPGlobals.mF40EC, "Writing Hoardings Balance file\n");
}

// 0x10095e54 (764 bytes). The same with the complete hoardings' file name.
int32_t LLCC_WriteCompleteHoardingsToBalanceFile() {
    return WriteBalanceFile(gTPGlobals.mF4100, "Writing Complete Hoardings Balance file\n");
}

// 0x10096190 (4 bytes): a bare blr in the original, so nothing is reset.
void LLCC_ResetBuyLandList() {}

// 0x100961c0, 424 bytes: redraws the hoarding as the rectangle of the two corners (in any order): the cell system's mode is 0x19 and the hoarding
// flag byte 1 while the five clicks go round the rectangle; then the database is built from the extents (its answer kept), the hoarding is locked,
// the inside of the park filled and the hoarding unlocked, and the park's darkening recalculated.
void LLCC_ReviseHoarding(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    const int32_t lowX = x2 < x1 ? x2 : x1;
    const int32_t highX = x1 < x2 ? x2 : x1;
    const int32_t lowY = y2 < y1 ? y2 : y1;
    const int32_t highY = y1 < y2 ? y2 : y1;
    gLLCC4818 = 1;
    LLCC_SystemSetMode(0x19, false);
    LLCM_LeftMouseButtonUp(lowX, lowY, 0, 0);
    LLCM_LeftMouseButtonUp(highX, lowY, 0, 0);
    LLCM_LeftMouseButtonUp(highX, highY, 0, 0);
    LLCM_LeftMouseButtonUp(lowX, highY, 0, 0);
    LLCM_LeftMouseButtonUp(lowX, lowY, 0, 0);
    LLCC_SystemSetMode(0, false);
    UI_SetGameWindowCursor(1);
    gLLCCReviseAnswer = LLCC_BuildHoardingDB(gHoardingRight, gHoardingBottom, gHoardingLeft, gHoardingTop, 4, true);
    LLCC_LockHoarding(true);
    LLCC_FillInsideOfPark();
    LLCC_LockHoarding(false);
    RecalculateParkDarkening();
    gLLCC4818 = 0;
}

// 0x10096398, 712 bytes: fills the new image from (x, y) with `fill` (oldValue is never read): a cell is open when it is in the grid (0 to 0x41 each
// way) and holds ' ' or '@'. The cell itself, then four blocks (left, right, above, below), each marking its neighbour and recursing with the four calls
// the listing gives (several of them on cells already filled). A fill of ' ' or '@' would never end.
void LLCC_FloodFill(int32_t x, int32_t y, uint8_t oldValue, uint8_t fill) {
    uint8_t* image = NewImage();
    if (x > -1 && x < 0x42 && y > -1 && y < 0x42) {
        const int32_t row = x * kImageSide;
        uint8_t* cell = image + row + y;
        uint8_t value = *cell;
        if (value == ' ' || value == '@') {
            *cell = fill;
            if (x - 1 > -1) {
                if (x - 1 < 0x42 && y > -1 && y < 0x42) {
                    cell = image + row + y - kImageSide;
                    value = *cell;
                    if (value == ' ' || value == '@') {
                        *cell = fill;
                        LLCC_FloodFill(x - 2, y, oldValue, fill);
                        LLCC_FloodFill(x, y, oldValue, fill);
                        LLCC_FloodFill(x - 1, y - 1, oldValue, fill);
                        LLCC_FloodFill(x - 1, y + 1, oldValue, fill);
                    }
                }
            }
            if (x + 1 > -1 && x + 1 < 0x42 && y > -1 && y < 0x42) {
                cell = image + row + y + kImageSide;
                value = *cell;
                if (value == ' ' || value == '@') {
                    *cell = fill;
                    LLCC_FloodFill(x, y, oldValue, fill);
                    LLCC_FloodFill(x + 2, y, oldValue, fill);
                    LLCC_FloodFill(x + 1, y - 1, oldValue, fill);
                    LLCC_FloodFill(x + 1, y + 1, oldValue, fill);
                }
            }
            if (x > -1) {
                if (x < 0x42 && y - 1 > -1 && y - 1 < 0x42) {
                    cell = image + row + y - 1;
                    value = *cell;
                    if (value == ' ' || value == '@') {
                        *cell = fill;
                        LLCC_FloodFill(x - 1, y - 1, oldValue, fill);
                        LLCC_FloodFill(x + 1, y - 1, oldValue, fill);
                        LLCC_FloodFill(x, y - 2, oldValue, fill);
                        LLCC_FloodFill(x, y, oldValue, fill);
                    }
                }
            }
            if (x > -1) {
                if (x < 0x42 && y + 1 > -1 && y + 1 < 0x42) {
                    cell = image + row + y + 1;
                    value = *cell;
                    if (value == ' ' || value == '@') {
                        *cell = fill;
                        LLCC_FloodFill(x - 1, y + 1, oldValue, fill);
                        LLCC_FloodFill(x + 1, y + 1, oldValue, fill);
                        LLCC_FloodFill(x, y, oldValue, fill);
                        LLCC_FloodFill(x, y + 2, oldValue, fill);
                    }
                }
            }
        }
    }
}

// 0x1009668c, 372 bytes: the current image from the world, one byte per two by two cells (the last cell of the pair wins): ' ' first, then '@' for a
// hoarding cell, else '#' unless the cell's map cell is flagged 0x40 or the byte already holds '@'. A track X cell stands for its parent. The bounds
// are read again on every pass.
void LLCC_CreateCurrentHoardingImage() {
    uint8_t* image = CurrentImage();
    for (int32_t y = 0; y < gLLCCHeight; ++y) {
        for (int32_t x = 0; x < gLLCCWidth; ++x) {
            uint8_t* byte = image + (x >> 1) * kImageSide + (y >> 1);
            *byte = ' ';
            if (CMap::IsXYValid(x, y) != 0) {
                CTrackCell* cell = TrackCellOfId(CellId(x, y));
                if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
                CMapCell* mapCell = cell->GetMapCellEquiv();
                if (cell->IsHoarding() != 0) {
                    *byte = '@';
                } else if (mapCell->IsFlag(0x40) == 0 && *byte != '@') {
                    *byte = '#';
                }
            }
        }
    }
}

// 0x10096838, 108 bytes: the current image shifted one row and one column (+0x44) into the new one, 0x42 by 0x42 bytes, with the new image's border
// (first and last row and column) set to ' ' on the way (the outer loop is the column of the source, the inner one its row; the last byte written is 0x1188).
void LLCC_CopyCurrentToNewImage() {
    uint8_t* newImage = NewImage();
    uint8_t* current = CurrentImage();
    for (int32_t i = 0; i < 0x42; ++i) {
        for (int32_t j = 0; j < 0x42; ++j) {
            newImage[j * kImageSide + i + 0x44] = current[j * kImageSide + i];
        }
        newImage[i] = ' ';
        newImage[i + 0x1103] = ' ';
        newImage[i * kImageSide] = ' ';
        newImage[i * kImageSide + 0x41] = ' ';
    }
}

// 0x100968d8, 80 bytes: '#' into the new image for every second cell of the area (x1 + 1 to x2 + 1, y1 + 1 to y2 + 1, step 2), at (x >> 1, y >> 1).
void LLCC_AddNewAreaToImage(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    uint8_t* image = NewImage();
    for (int32_t y = y1 + 1; y <= y2 + 1; y += 2) {
        for (int32_t x = x1 + 1; x <= x2 + 1; x += 2) {
            image[(x >> 1) * kImageSide + (y >> 1)] = '#';
        }
    }
}

// 0x10096958, 140 bytes: fills the new image from the corner (0x41, 0) with '#' and answers 0 when a ' ' or '@' is left inside (index row * 0x43 + column,
// the outer loop on the column, 1 to 0x40 each way), else 1.
int32_t LLCC_SoIsItLegal() {
    uint8_t* image = NewImage();
    LLCC_FloodFill(0x41, 0, ' ', '#');
    for (int32_t outer = 1; outer < 0x41; ++outer) {
        for (int32_t inner = 1; inner < 0x41; ++inner) {
            const uint8_t value = image[inner * kImageSide + outer];
            if (value == ' ' || value == '@') return 0;
        }
    }
    return 1;
}

// 0x10096a0c, 8 bytes.
int32_t LLCC_IsClearLandLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    return 1;
}

// 0x10096a44, 340 bytes: counts the valid cells of the area (rows y1 to y2, columns x1 to x2) and those that are hoarding (a track X cell through its
// parent) or whose map cell is flagged 0x40; answers whether the two counts differ.
bool LLCC_IsPatrolAreaLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int32_t valid = 0;
    int32_t blocked = 0;
    for (int32_t y = y1; y <= y2; ++y) {
        for (int32_t x = x1; x <= x2; ++x) {
            if (CMap::IsXYValid(x, y) != 0) {
                CMapCell* mapCell = MapCellOfId(CellId(x, y));
                CTrackCell* cell = mapCell->GetTrackCellEquiv();
                if (cell->IsTrackX() != 0) cell = TrackCellOfId(cell->mParentID);
                if (cell->IsHoarding() != 0) {
                    ++blocked;
                } else if (mapCell->IsFlag(0x40) != 0) {
                    ++blocked;
                }
                ++valid;
            }
        }
    }
    return blocked != valid;
}

// 0x10096bc8, 772 bytes: turns the mouse position into the sprite hot point (x / 1024 - 1, y / 767.5 - 1), and when a sprite is hot and a script owns
// it, walks the thing lists of the cell under the pointer and its eight neighbours for the first person-like thing (kinds 1, 4 to 8, 0x12) whose
// script handle (+0xc) is that script. The cell id 0 gives x 0x7f and a huge y (unsigned shift), as on the Mac.
CThing* LLCC_SystemGetPointedPeepThing() {
    const uint16_t cellId = gIntrface.mF24;
    int16_t mouseX;
    int16_t mouseY;
    UI_GetMouseXY(mouseX, mouseY);
    gSpriteHotX = static_cast<float>(mouseX) * 0.0009765625f - 1.0f;
    gSpriteHotY = static_cast<float>(mouseY) / 767.5f - 1.0f;
    SSpriteGameData* sprite = gHotSprite;
    if (sprite == nullptr) return nullptr;
    CTRACE(6, "Found sprite %lx\n", sprite);
    int32_t script = SCR_WhichScriptHasThisSprite(sprite);
    if (script == 0) return nullptr;
    CTRACE(6, " Found script %lx\n", script);
    const int32_t m = static_cast<int32_t>(cellId) - 1;
    const int32_t baseX = m & 0x7f;
    const int32_t baseY = static_cast<int32_t>(static_cast<uint32_t>(m) >> 7);
    for (int32_t i = 0; i < 9; ++i) {
        const int32_t x = baseX + gLLCCPointedOffsets[i].mX;
        const int32_t y = baseY + gLLCCPointedOffsets[i].mY;
        if (CMap::IsXYValid(x, y) == 0) continue;
        const uint16_t id = static_cast<uint16_t>(y * 128 + 1 + x);
        uint16_t who = gGameWorld->mMapCells[static_cast<int32_t>(id) - 1].mWho;
        while (who != gLLCCThingListEnd) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            uint8_t* thing = static_cast<uint8_t*>(gThingArray[who].mThing);
            const uint8_t kind = thing[2];
            if (kind == 1 || (kind >= 4 && kind <= 8) || kind == 0x12) {
                const int32_t handle = *reinterpret_cast<int32_t*>(thing + 0xc);
                CTRACE(6, "  Checking against scripts %lx\n", handle);
                if (handle == script) return reinterpret_cast<CThing*>(thing);
            }
            who = *reinterpret_cast<uint16_t*>(thing + 0xa);
        }
    }
    return nullptr;
}
