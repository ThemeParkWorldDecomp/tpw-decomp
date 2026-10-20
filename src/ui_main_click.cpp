// ui_main.cpp (main app), unit w143: ContextMouseClick (0x10183320, 2,364 bytes), the click in the park. gGameWinWordC38 is the KIND of the help
// text the last ContextMouse call showed (src/ui_main_context.cpp): the click acts on what the pointer is over. Answers 0 (the click is used up:
// a ride is placed or the coaster editor opens) or 1; nothing happens in a menu or without a world.
//   kind 1, 2   a path is drawn (CMMPathDraw mode 1), when the pointer is over a cell
//   kind 3      a cell of type 3: its object is selected, the path mode 0x14
//   kind 4      an upgradable: with the command key held and the ride affordable the path mode 4 for its object type (answers 0), else its panel
//   kind 5      a track cell (types 0xb, 0x10, 0x19): the path mode 0xe, 0x13 or 0x1a
//   kind 6      the coaster editor for the object of the cell (answers 0)
//   kind 7      the guest panel, 8 to 0xc the staff panel, 0xd and 0xe the entry price panel
// The null ids are halfwords of the image (0x102a76d8, 0x102a78d0, 0x102a78d2, 0x102a78d4, 0x102a78d6, all 0 there); the names are invented.
#include <cstdint>
#include <cstring>
#include <new>

#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "ui_main.h"

extern const uint16_t gClickNoCell = 0;       // 0x102a76d8: the cell id of "no cell"
extern const uint16_t gClickNoResult = 0;     // 0x102a78d0: what LLCR_OverRide answers for a column without a cell
extern const uint16_t gClickNoThing3 = 0;     // 0x102a78d2: the thing id of "no thing" (kind 3)
extern const uint16_t gClickNoThing6 = 0;     // 0x102a78d4: the same (kind 6)
extern const uint16_t gClickNoThing5 = 0;     // 0x102a78d6: the same (kind 5)

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024d8b2 + 6

// The Mac makes the mouse and its default mode in this order inline (see src/ui_main_context.cpp / ui_main_panel.cpp): the mouse, then the
// default mode, which the mouse takes; then the two words of the mouse's data are set to -1. The answer is NULL when the mouse could not be made.
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

// The new mode goes to the mouse (made first if there is none; the Mac calls the assignment even for a missing mouse).
void GoToMode(CMouseMode* mode) {
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);
}

CMapCell* MapCellOf(uint32_t id) {   // the 1 based cell id
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + static_cast<uint32_t>((id - 1) * 0x44u));
}
CTrackCell* TrackCellOf(uint32_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + 0x110000 + static_cast<uint32_t>((id - 1) * 0x28u));
}

// The thing of a thing id, after the Mac's assertion on row 0.
void* ThingOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return gThingArray[id].mThing;
}

// A path drawing mode of the cell mode, handed to the mouse.
void PathDraw(int32_t cellMode) {
    CMMPathDraw* mode = new (std::nothrow) CMMPathDraw(static_cast<CELL_MODES>(cellMode));
    GoToMode(mode);
}
}  // namespace

int32_t ContextMouseClick() {
    if (gGameWorld == nullptr) return 0;
    if (MainMenuUp() != 0) return 0;
    if (gMouse == nullptr) gMouse = MakeMouse();
    int32_t current = 0;
    if (gMouseMode != nullptr) current = gMouseMode->GetMode();
    if (static_cast<uint32_t>(current) > 1) return 1;
    UI_CheckSelected();
    const uint32_t kind = static_cast<uint32_t>(gGameWinWordC38);
    if (kind > 0xe) return 1;
    switch (kind) {
        case 1:
        case 2: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            PathDraw(1);
            return 1;
        }
        case 3: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            const CMapCell* cell = MapCellOf(id);
            if (static_cast<int32_t>(cell->mType) != 3) return 1;
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(cell->mParentID)->GetObjectThingId();
            if (thing.mIndex == gClickNoThing3) return 1;
            UI_SetSelectedObject(ThingOf(thing.mIndex));
            LLCM_SetCurrentObject(true);
            PathDraw(0x14);
            return 1;
        }
        case 4: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            uint16_t found = 0;
            const uint32_t cellIndex = static_cast<uint32_t>(id) - 1;
            LLCR_OverRide(&found, static_cast<int32_t>(cellIndex & 0x7f), static_cast<int32_t>(cellIndex >> 7));
            if (found == gClickNoResult) return 1;
            if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new (std::nothrow) SamsUtilities::UKeyboard;
            const uint8_t commandDown = SamsUtilities::gKeyboard->IsCommandDown();
            if (commandDown != 0) {
                const uint8_t* const object = static_cast<const uint8_t*>(ThingOf(found));
                uint16_t objectType;
                std::memcpy(&objectType, object + 0xe, sizeof(objectType));
                int32_t rotation;
                std::memcpy(&rotation, object + 0x10, sizeof(rotation));
                const ObjectControl* const control =
                    gGameWorld->GetControlManager()->GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{objectType}, 0);
                int32_t price;
                price = control->mUpgradeCost;
                const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                if (price > balance) {
                    SOUND_SendEvent(0, gSoundBanks[3], 0x1d, 0, 0, 0);
                } else {
                    CMMPathDraw* mode = new (std::nothrow) CMMPathDraw(static_cast<CELL_MODES>(4), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{objectType});
                    GoToMode(mode);
                    LLCC_SetRideRotation(rotation, true);
                }
                return 0;
            }
            UIMain_GotoStatusPanelForThing(UI_GetSelectedObject());
            return 1;
        }
        case 5: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            const CTrackCell* track = TrackCellOf(id);
            if (static_cast<int32_t>(track->mType) == 0xc) track = TrackCellOf(track->mParentID);
            const int32_t type = static_cast<int32_t>(track->mType);
            int32_t pathMode;
            if (type == 0x10) {
                pathMode = 0x13;
            } else if (type == 0xb) {
                pathMode = 0xe;
            } else if (type == 0x19) {
                LLCM_SetCurrentObject(true);
                PathDraw(0x1a);
                return 1;
            } else {
                return 1;
            }
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(track->mParentID)->GetObjectThingId();
            if (thing.mIndex == gClickNoThing5) return 1;
            UI_SetSelectedObject(ThingOf(thing.mIndex));
            LLCM_SetCurrentObject(true);
            PathDraw(pathMode);
            return 1;
        }
        case 6: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(MapCellOf(id)->mParentID)->GetObjectThingId();
            if (thing.mIndex == gClickNoThing6) return 1;
            void* object = ThingOf(thing.mIndex);
            UI_SetSelectedObject(object);
            CoasterEditMode(object);
            return 0;
        }
        case 7: {
            CThing* peep = UI_GetSelectedPeep();
            if (peep != nullptr) gVisitorStatusPanel.Show(peep);
            return 1;
        }
        case 8:
        case 9:
        case 10:
        case 11:
        case 12: {
            CThing* peep = UI_GetSelectedPeep();
            if (peep != nullptr) gStaffStatusPanel.Show(peep);
            return 1;
        }
        case 13:
        case 14:
            CreateEntryPricePanel();
            return 1;
        default:
            return 1;
    }
}
