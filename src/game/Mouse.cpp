// Mouse.cpp: name proven by __sinit_Mouse_cpp in the main application.

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "intrface.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "online.h"
#include "platform/arena.h"
#include "RideDB.h"
#include "StaffDB.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <new>

// Mouse.cpp (main app), unit w142: CMouse's three handlers the frame and the window messages call, and DoubleClick. All four hand the
// pointer to the current mouse mode (gMouseMode, re-read at every call, never tested for NULL) through its vtable.

// The two button flags of CMouse's data (names invented): ProcessIngameMouse sets the left's (0x102dc1ec) and the right's (0x102dc1e8),
// ImmediateMouseHandler passes them to OnMouseMove and picks WhileLeftDown or WhileRightDown by them.
int32_t gMouseLeftDown;    // 0x102dc1ec
int32_t gMouseRightDown;   // 0x102dc1e8

// 0x1007de84 (248 bytes): key 0xf0 is the left button, 0xf1 the right; `down` 1 presses, anything else releases. Any other key does
// nothing. The Mac answers 1 in r3 on every path; the callers ignore it, so it is void here. `data` is not used.
void CMouse::ProcessIngameMouse(int32_t key, int32_t down, int32_t x, int32_t y, void* data) {
    (void)data;
    if (key == 0xf0) {
        if (down == 1) {
            gMouseMode->OnLeftClick(x, y);
            gMouseLeftDown = 1;
        } else {
            gMouseMode->OnLeftRelease(x, y);
            gMouseLeftDown = 0;
        }
    } else if (key == 0xf1) {
        if (down == 1) {
            gMouseMode->OnRightClick(x, y);
            gMouseRightDown = 1;
        } else {
            gMouseMode->OnRightRelease(x, y);
            gMouseRightDown = 0;
        }
    }
}

// 0x1007e094 (216 bytes). `this` is not used (the Mac's r3 is gMouse). The positions compare as signed halfwords-in-words. The last
// position's two words both get x (the Mac stores x at 0x102dc1e4 and again at 0x102dc1e0, never y).
void CMouse::ImmediateMouseHandler(int32_t x, int32_t y) {
    if (x != gMouseWord1E4 || y != gMouseWord1E0) {
        gMouseMode->OnMouseMove(x, y, gMouseLeftDown, gMouseRightDown);
    }
    if (gMouseLeftDown != 0) {
        gMouseMode->WhileLeftDown(x, y);
    } else if (gMouseRightDown != 0) {
        gMouseMode->WhileRightDown(x, y);
    }
    gMouseWord1E4 = x;
    gMouseWord1E0 = x;
}

// 0x1007e248 (52 bytes).
void CMouse::DoubleClick(int32_t x, int32_t y) {
    gMouseMode->OnDoubleClick(x, y);
}

// Mouse.cpp (main): AssignMouseMode 0x1007e2e0 (312 B), CMMDefault constructor 0x1007ea78 (36 B),
// destructor 0x100813e8 (88 B), clicks 0x1007eac4 / 0x1007eaf8 (4 B each), GetMode 0x10081468 (8 B), OnAssign 0x1007eb2c (60 B).

CMouse* gMouse;                 // 0x102dc1f4
CMouseMode* gMouseMode;         // 0x102dc1f0
int32_t gMouseWord1E0;          // 0x102dc1e0
int32_t gMouseWord1E4;          // 0x102dc1e4

void CMouse::AssignMouseMode(CMouseMode* mode) {
    if (gMouseMode != nullptr) {
        gMouseMode->OnRelease();
        // OnRelease can replace or clear the global.
        if (gMouseMode != nullptr) delete gMouseMode;
    }
    if (TheGameType().mType == kGameTypeOnline && mode != nullptr) {
        if (mode->GetMode() == 1) {
            delete mode;
            mode = new (std::nothrow) CMMOnline;
        }
    }
    gMouseMode = mode;
    if (mode != nullptr) mode->OnAssign();
}

// Mouse.cpp (main app), unit w143: CSpritePlaceMode (the base of the two staff modes), CMMPlaceStaff and CMMPickUpStaff. The modes are made
// by the staff panel (not translated yet). Every member name and every global added here is invented; the layouts are the Mac's (Mouse.h).
//   CSpritePlaceMode: OnMouseMove 0x1007e528 (388 bytes), the destructor 0x1007ec14 (88), OnRelease 0x10081550 (68)
//   CMMPlaceStaff:    constructor 0x1007eb94 (60), destructor 0x10081320 (100), GetMode 0x100813b0 (8), OnAssign 0x1007ec9c (492),
//                     OnRelease 0x1007eeb8 (152), OnLeftClick 0x1007ef80 (1,208)
//   CMMPickUpStaff:   constructor 0x1007f46c (48), destructor 0x10081258 (140), GetMode 0x100812e8 (8), OnAssign 0x1007f4d8 (132),
//                     OnLeftClick 0x1007f58c (904), OnRightClick 0x1007fa74 (4), OnRelease 0x1007f948 (252)
// The destructors only rewrite the vtable words on the Mac (and free `this` for a positive flag, which is the delete expression's job here).

// The null cell id of CStaffHQ::mF60 (a const halfword of the image at 0x1029d434; its value there is 0). The name is invented.
extern const uint16_t gStaffHQNullId = 0;

namespace {
// The data block ESP_SpawnScript takes as its first argument (0x1029f768, Mouse.cpp's data; its first words are 0x102669d8, 0x10, 0x1200,
// 0x10266a50, 6, 0x10266a60, 0, 0x10266a60, 1 ...: pointers into the TOC area and small numbers, meaning unknown). The host gives it an opaque
// block of the same first words under an invented name; ESP_SpawnScript is untranslated.
uint32_t gStaffSpawnBlock[12] = {0x102669d8, 0x10, 0x1200, 0x10266a50, 6, 0x10266a60, 0, 0x10266a60, 1, 0x10266a60, 2, 0x10266a60};

constexpr const char* kUnknownStaffPlace = "CMMPlaceStaff: placing unknown staff type";                 // 0x1023df1b
constexpr const char* kUnknownStaffClick = "CMMPlaceStaff::OnLeftClick() - unknown staff type";       // 0x1023df45
constexpr const char* kCannotPlace = "Cannot place staff member here - the cell is of type %d\n";     // 0x1023df77
constexpr const char* kCannotPut = "Can't put staff here";                                             // 0x1023dfb0
constexpr const char* kDropping = "Dropping staff where he was before he was picked up";               // 0x1023dfc5
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023dff9

constexpr int32_t kCursorPlace = 9;   // SetGameWindowCursor's argument

// A staff member of class T on the game heap (the Mac's operator new): the arena on the host (unit v5L31), since the lists keep a thing's
// address in 32 bits. Nothing is made when the arena is used up, as the Mac's new answers NULL.
template <class T, class... Args>
void NewStaff(Args... args) {
    if (void* block = tpw::ArenaAlloc(sizeof(T))) new (block) T(args...);
}

// The map cell of the pointer (gIntrface.mF24, the 1 based id): plain arithmetic on the world, as the Mac does it.
CMapCell* PointerCell() {
    const uint16_t id = gIntrface.mF24;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + static_cast<uint32_t>((id - 1) * 0x44u));
}

// The Mac makes the mouse and its default mode in this order inline: the mouse (an empty class on the Mac, operator new(1)), then the default
// mode (operator new(8)), which the mouse takes; then the two words of the mouse's data are set to -1. A mouse that could not be made leaves
// the words alone. The answer is the mouse, NULL when it could not be made.
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

// Puts the default mode back: it is made first, then the mouse if there is none, then the mouse takes the mode (and deletes the current one,
// which is `this` of the caller: nothing may touch a member afterwards). The Mac calls the assignment even for a missing mouse.
void AssignDefaultMode() {
    CMMDefault* mode = new (std::nothrow) CMMDefault;
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);
}

// The script a mode spawns: the old handle is killed AFTER the new script exists.
void ReplaceScript(int32_t& handle, int32_t fresh) {
    if (handle != 0) SCR_KillScript(handle);
    handle = fresh;
}
}  // namespace

// 0x1007e528. The script follows the pointer (the point lifted 5.0f); over a track, a flag, a hoarding, or a cell that is neither grass, a path
// nor a queue a square face is drawn under the point (5.0f to the left and to the front, one unit up).
void CSpritePlaceMode::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {
    if (mScript == 0) return;
    const s3DPoint* const picked = GetPickupWorldPosn();
    const float x = picked->x;
    float y = picked->y;
    const float z = picked->z;
    y = y + 5.0f;
    CMapCell* const cell = PointerCell();
    SCR_SetScriptPosition(mScript, x, y, z, 4);
    bool draw = false;
    if (cell->IsTrack() != 0) {
        draw = true;
    } else if (cell->IsFlag(0x40) != 0) {
        draw = true;
    } else if (cell->IsHoarding() != 0) {
        draw = true;
    } else {
        if (cell->IsGrass() != 0) return;
        if (cell->IsPath() != 0) return;
        if (cell->IsQueue() != 0) return;
        draw = true;
    }
    if (draw) {
        DC_AddSquareFaceToList(x - 5.0f, 1.0f, z - 5.0f, 0, 1, 0, 1.0f, 0, 0, 0, static_cast<DYN_Type>(0));
    }
}

// Mouse.cpp (main app), unit w144: the rest of the file: CMMPathDraw, CMMOnline, CMMCoaster, CMMFirstPersonTarget and __sinit_Mouse_cpp. The ten
// 4 byte bodies of the base class CMouseMode (0x1007dfbc, 0x1007dff4, 0x1007e028, 0x1007e060, 0x1007e1a0, 0x1007e1d8, 0x1007e210, 0x1007e2a8,
// 0x1007e454, 0x1007e4f8) and its destructor 0x1007e484 are inline in include/Mouse.h. Every member name and every global added here is invented.
//   CMMPathDraw:  constructors 0x1007e6e4 (44) and 0x1007e744 (56), OnLeftClick 0x1007e7e0 (128), OnLeftRelease 0x1007e890 (128),
//                 OnRightClick 0x1007e944 (4), WhileLeftDown 0x1007e97c (4), WhileRightDown 0x1007e9b4 (4), OnMouseMove 0x1007e9ec (88),
//                 OnAssign 0x10080ee4 (284), destructor 0x1008149c (88), GetMode 0x1008151c (8)
//   CMMOnline:    constructor 0x1007faac (88), OnLeftClick 0x1007fb2c (124), OnRightClick 0x1007fbd8 (4), OnDoubleClick 0x1007fc0c (124),
//                 OnAssign 0x1007fcb8 (48), destructor 0x100811a8 (88), GetMode 0x10081228 (8)
//   CMMCoaster:   constructor 0x1007fd14 (28), OnLeftClick 0x1007fd58 (40), OnLeftRelease 0x1007fdb0 (40), OnRightClick 0x1007fe0c (4),
//                 OnAssign 0x1007fe40 (4), destructor 0x100810f4 (88), GetMode 0x10081174 (8)
//   CMMFirstPersonTarget: constructor 0x1007fe70 (28), OnMouseMove 0x1007fec0 (416), OnLeftClick 0x1008009c (3,432), OnRightClick 0x10080e40 (4),
//                 OnAssign 0x10080e80 (44), destructor 0x1008102c (88), GetMode 0x100810b8 (8)
//   __sinit_Mouse_cpp 0x100815c8 (28): the two words after gMouse (constant initialisation here)
// The destructors only rewrite the vtable words on the Mac (and free `this` for a positive flag, which is the delete expression's job here).

// __sinit_Mouse_cpp (0x100815c8) stores these two words on the Mac; the host initialises them at load time. The names are invented.
int32_t gMouseWord1F8 = 0x10000;   // 0x102dc1f8
int32_t gMouseWord1FC = 0;         // 0x102dc1fc

// The null ids (const halfwords of the image, both 0; the names are invented): the pointer's "no cell" id and LLCR_OverRide's "no ride" answer.
extern const uint16_t gPointerNullId = 0;    // 0x1029d432
extern const uint16_t gOverRideNullId = 0;   // 0x1029d436

namespace {
constexpr const char* kAssigningOnline = "Assigning CMMOnline\n";   // 0x1023e03c
constexpr uint16_t kFlagLowered = 0x40;    // CMapCell::IsFlag's arguments (meaning not known)
constexpr uint16_t kFlagWalkable = 0x80;
constexpr int32_t kCursorRefused = 8;      // SetGameWindowCursor's arguments
constexpr int32_t kCursorTarget = 0x13;

// The cell system's coordinates of the pointer cell id (1 based; the arithmetic is 32 bit, so id 0 gives 0x7f and 0x1ffffff).
int32_t PointerX(uint16_t id) { return static_cast<int32_t>((static_cast<uint32_t>(id) - 1) & 0x7f); }
int32_t PointerY(uint16_t id) { return static_cast<int32_t>((static_cast<uint32_t>(id) - 1) >> 7); }

// The map cell and the track cell of a 1 based id: plain arithmetic on the world, as the Mac does it (a null world is not tested).
CMapCell* MapCellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
}
CTrackCell* TrackCellOfId(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// The hand over of OnLeftClick (four inline copies on the Mac): the next mode is made first, and for the online game it is the online mode
// itself (not a default mode swapped by AssignMouseMode: its constructor's cursor call comes before anything of the old mode is released),
// then the mouse if there is none, then the mouse takes the mode (and releases and deletes the current one, which is `this` of the caller:
// nothing may touch a member afterwards). The Mac calls the assignment even for a missing mouse.
void HandOverMouse() {
    CMouseMode* mode;
    if (TheGameType().mType == kGameTypeOnline) {
        mode = new (std::nothrow) CMMOnline;
    } else {
        mode = new (std::nothrow) CMMDefault;
    }
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);
}

// The online control, made on first use (src/GameSys.cpp has the same idiom, file-local): operator new(0xe8c) and the constructor when it
// could be allocated; the global takes the pointer whatever it is.
COnlineControl* OnlineControl() {
    if (gOnlineControl == nullptr) {
        COnlineControl* control = new (std::nothrow) COnlineControl;
        gOnlineControl = control;
    }
    return gOnlineControl;
}
}  // namespace

// ---- CMMPathDraw (vtable 0x1029d640, 0x14 bytes)
// The Mac stores the base vtable, mWord4 = 0, its own vtable and mWord4 = 1; the first constructor leaves +0x10 alone.
CMMPathDraw::CMMPathDraw(CELL_MODES mode) {
    mWord4 = 1;
    mCellMode = mode;
    mF0C = 0;
}

CMMPathDraw::CMMPathDraw(CELL_MODES mode, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    mWord4 = 1;
    mCellMode = mode;
    mF0C = 1;
    mObjectId = id.mIndex;
}

// 0x1007e7e0 and 0x1007e890: the pointer's cell with the shift key state (masked to 16 bits) and 1.
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    const int32_t x = PointerX(id);
    const int32_t y = PointerY(id);
    const uint16_t shift = static_cast<uint16_t>(gUIInterface.GetShiftKeyState());
    LLCM_LeftMouseButtonDown(x, y, shift, 1);
}

void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    const int32_t x = PointerX(id);
    const int32_t y = PointerY(id);
    const uint16_t shift = static_cast<uint16_t>(gUIInterface.GetShiftKeyState());
    LLCM_LeftMouseButtonUp(x, y, shift, 1);
}

void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}

void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    LLCM_MouseMoved(PointerX(id), PointerY(id), 0);
}

CMMDefault::CMMDefault() { mWord4 = 1; }
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
void CMMDefault::OnAssign() {
    gUIInterface.CheckMouseContext(1);
    CTRACE(3, "Assigning CMMDefault\n");   // 0x1023df05
}

// ---- CMMPlaceStaff

CMMPlaceStaff::CMMPlaceStaff(int32_t model, uint8_t payGrade, uint8_t flag, int32_t value)
    : mModel(model), mPayGrade(payGrade), mF11(flag), mF14(value), mPlaced(0) {}

// ---- CSpritePlaceMode

CSpritePlaceMode::~CSpritePlaceMode() {}

// 0x1007ec9c: the cursor, then the script of the staff kind's sprite (the sprite number is not the model: 4 -> 6, 5 -> 5, 6 -> 4, 7 -> 7,
// 8 -> 8). Any other model asserts and spawns nothing.
void CMMPlaceStaff::OnAssign() {
    gUIInterface.SetGameWindowCursor(kCursorPlace);
    int32_t sprite = 0;
    switch (mModel) {
        case 6: sprite = 4; break;
        case 5: sprite = 5; break;
        case 4: sprite = 6; break;
        case 7: sprite = 7; break;
        case 8: sprite = 8; break;
        default:
            GAME_ASSERTMSG(0, kUnknownStaffPlace);
            return;
    }
    const int32_t handle = ESP_SpawnScript(gStaffSpawnBlock, static_cast<ESP_Sprites>(sprite), mF11, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
    ReplaceScript(mScript, handle);
}

// 0x1007eeb8: kills the script; the staff database is told what became of the staff member (0 once placed, else 1); the advisor drops the
// message 0x135.
void CMMPlaceStaff::OnRelease() {
    if (mScript != 0) SCR_KillScript(mScript);
    mScript = 0;
    CStaffDB* const database = reinterpret_cast<CStaffDB*>(gGameWorld);   // the database is the first member of the world
    if (mPlaced != 0) {
        database->FateOfPickedUpStaff(static_cast<CStaffDB::StaffFate>(0));
    } else {
        database->FateOfPickedUpStaff(static_cast<CStaffDB::StaffFate>(1));
    }
    gGameWorld->GetAdvisor()->CancelMessage(static_cast<AdvisorMessage>(0x135));
}

// 0x1007ef80: the cell under the pointer takes the staff member when it is not a track, a flag or a hoarding and is grass, a path or a queue;
// the staff object is made by its model, the mode goes back to the default (which deletes this mode).
void CMMPlaceStaff::OnLeftClick(int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    CMapCell* const cell = PointerCell();
    bool refused = false;
    if (cell->IsTrack() != 0) {
        refused = true;
    } else if (cell->IsFlag(0x40) != 0) {
        refused = true;
    } else if (cell->IsHoarding() != 0) {
        refused = true;
    } else if (cell->IsGrass() == 0 && cell->IsPath() == 0 && cell->IsQueue() == 0) {
        refused = true;
    }
    if (refused) {
        CTRACE(2, kCannotPlace, static_cast<int32_t>(cell->mType));
        return;
    }
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellId{id};
    switch (mModel) {
        case 5: NewStaff<CHandyman>(cellId, mPayGrade, mF11, mF14); break;
        case 6: NewStaff<CEntertainer>(cellId, mPayGrade, mF11, mF14); break;
        case 7: NewStaff<CGuard>(cellId, mPayGrade, mF11, mF14); break;
        case 4: NewStaff<CMechanic>(cellId, mPayGrade, mF11, mF14); break;
        case 8: NewStaff<CResearcher>(cellId, mPayGrade, mF11, mF14); break;
        default: GAME_ASSERTMSG(0, kUnknownStaffClick); break;
    }
    mPlaced = 1;
    AssignDefaultMode();
}

// ---- CMMPickUpStaff

CMMPickUpStaff::CMMPickUpStaff(ESP_Sprites sprite, uint8_t flag) : mSprite(sprite), mF10(flag) {}

// 0x1007f4d8
void CMMPickUpStaff::OnAssign() {
    gUIInterface.SetGameWindowCursor(kCursorPlace);
    const int32_t handle = ESP_SpawnScript(gStaffSpawnBlock, mSprite, mF10, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
    ReplaceScript(mScript, handle);
}

// 0x1007f58c: the cell must not be a flag, a hoarding or a track (this order) and must be grass, a path or a queue: the staff headquarters
// drops the staff member there and the mode goes back to the default (which deletes this mode).
void CMMPickUpStaff::OnLeftClick(int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    CMapCell* const cell = PointerCell();
    bool refused = false;
    if (cell->IsFlag(0x40) != 0) {
        refused = true;
    } else if (cell->IsHoarding() != 0) {
        refused = true;
    } else if (cell->IsTrack() != 0) {
        refused = true;
    } else if (cell->IsGrass() == 0 && cell->IsPath() == 0 && cell->IsQueue() == 0) {
        refused = true;
    }
    if (refused) {
        CTRACE(2, kCannotPut);
        return;
    }
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellId{id};
    gGameWorld->GetStaffHQ()->DropStaff(cellId);
    AssignDefaultMode();
}

// 0x1007f948: kills the script; a staff member still held by the headquarters (mF60 is not the null id) is put back in the cell he was picked
// up from (the thing's cell x and y, 1 based).
void CMMPickUpStaff::OnRelease() {
    if (mScript != 0) SCR_KillScript(mScript);
    mScript = 0;
    const uint16_t held = gGameWorld->GetStaffHQ()->mF60;
    if (held != gStaffHQNullId) {
        CTRACE(2, kDropping);
        const uint16_t thingId = gGameWorld->GetStaffHQ()->mF60;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        const CThing* const thing = static_cast<const CThing*>(gThingArray[thingId].mThing);
        const uint16_t cell = static_cast<uint16_t>(thing->mCellX + (static_cast<uint32_t>(thing->mCellY) << 7) + 1);
        gGameWorld->GetStaffHQ()->DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cell});
    }
}
void CMMPickUpStaff::OnRightClick(int32_t, int32_t) {}

// ---- CMMOnline (vtable 0x1029d4ec, 8 bytes)
CMMOnline::CMMOnline() {
    mWord4 = 0;
    gUIInterface.SetGameWindowCursor(0);
}

// 0x1007fb2c and 0x1007fc0c: the pointer's cell id is read first, then the online control made if there is none.
void CMMOnline::OnLeftClick(int32_t, int32_t) {
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell{gIntrface.mF24};
    OnlineControl()->SetAvatarDestination(cell);
}

void CMMOnline::OnRightClick(int32_t, int32_t) {}

void CMMOnline::OnDoubleClick(int32_t, int32_t) {
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell{gIntrface.mF24};
    OnlineControl()->TeleportAvatar(cell);
}

void CMMOnline::OnAssign() { CTRACE(3, kAssigningOnline); }

// ---- CMMCoaster (vtable 0x1029d4a0, 8 bytes)
CMMCoaster::CMMCoaster() { mWord4 = 0; }
void CMMCoaster::OnLeftClick(int32_t, int32_t) { CoasterAcceptLeftMouseClick(1); }
void CMMCoaster::OnLeftRelease(int32_t, int32_t) { CoasterAcceptLeftMouseClick(0); }
void CMMCoaster::OnRightClick(int32_t, int32_t) {}
void CMMCoaster::OnAssign() {}

// ---- CMMFirstPersonTarget (vtable 0x1029d454, 8 bytes)
CMMFirstPersonTarget::CMMFirstPersonTarget() { mWord4 = 0; }
// 0x1007fec0: the pointer cell is told to the cell system, then the cursor shows whether the walk around can start there (0x13) or not (8).
void CMMFirstPersonTarget::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    if (id == gPointerNullId) return;
    LLCM_MouseMoved(PointerX(id), PointerY(id), 0);
    CMapCell* cell = MapCellOfId(id);
    if (cell == nullptr) {
        gUIInterface.SetGameWindowCursor(kCursorRefused);
        return;
    }
    if (cell->IsFlag(kFlagLowered) != 0 && (cell->IsFlag(kFlagWalkable) == 0 || cell->IsWalkable() == 0)) {
        gUIInterface.SetGameWindowCursor(kCursorRefused);
        return;
    }
    CTrackCell* track = cell->GetTrackCellEquiv();
    if (track->IsTrackX() != 0) track = TrackCellOfId(track->mParentID);
    if (track->IsHoarding() != 0) {
        gUIInterface.SetGameWindowCursor(kCursorRefused);
    } else {
        gUIInterface.SetGameWindowCursor(kCursorTarget);
    }
}

// 0x1008009c. Part one: a ride under the pointer whose record has no UI type is ridden in the first person (GoFirstPerson) and the mouse goes back
// to the default mode. Part two (the pointer cell is not a ride, or the ride has a UI type): the walk around starts on a cell that can take
// it, and the mouse goes back to the default mode; the cursor is 0 then, or 8 when the cell is refused. The paths that hand the mouse over
// delete `this`, so the cursor calls use globals only. Both parts read the pointer's id from the global again.
void CMMFirstPersonTarget::OnLeftClick(int32_t, int32_t) {
    gUIInterface.SetGameWindowCursor(1);
    const uint16_t id = gIntrface.mF24;
    if (id != gPointerNullId) {
        uint16_t overRide;
        LLCR_OverRide(&overRide, PointerX(id), PointerY(id));
        if (overRide != gOverRideNullId) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            CObject* object = static_cast<CObject*>(gThingArray[overRide].mThing);
            if (object->GetDBInfo()->mUIType == 0) {
                object->GoFirstPerson(0);
                HandOverMouse();
                return;
            }
        }
    }
    const uint16_t cellId = gIntrface.mF24;
    if (cellId == gPointerNullId) return;
    CMapCell* cell = MapCellOfId(cellId);
    if (cell == nullptr) {
        gUIInterface.SetGameWindowCursor(kCursorRefused);
        return;
    }
    if (cell->IsFlag(kFlagLowered) != 0 && (cell->IsFlag(kFlagWalkable) == 0 || cell->IsWalkable() == 0)) {
        gUIInterface.SetGameWindowCursor(kCursorRefused);
        return;
    }
    CTrackCell* track = cell->GetTrackCellEquiv();
    if (track->IsTrackX() != 0) track = TrackCellOfId(track->mParentID);
    if (track->IsHoarding() != 0) {
        gUIInterface.SetGameWindowCursor(kCursorRefused);
        return;
    }
    ToggleFirstPersonWalkAround();
    HandOverMouse();
    gUIInterface.SetGameWindowCursor(0);
}
void CMMFirstPersonTarget::OnRightClick(int32_t, int32_t) {}
void CMMFirstPersonTarget::OnAssign() { gUIInterface.SetGameWindowCursor(kCursorTarget); }

// 0x10080ee4: the ride record (balance slots 159, 36 and 13 at +0x288, +0x9c and +0x40 on the Mac) is only read when mF0C is set; a NULL record
// is not tested by the original.
void CMMPathDraw::OnAssign() {
    RideInfo* info = nullptr;
    if (mF0C != 0) info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectId});
    const uint16_t id = gIntrface.mF24;
    LLCM_MouseMoved(PointerX(id), PointerY(id), 0);
    LLCC_SystemSetMode(static_cast<int32_t>(mCellMode), true);
    if (mF0C != 0) {
        int32_t track = 0;
        if (info->Value(kRideSlot_UpgradeType).mInteger == 1) track = info->Value(kRideSlot_WhichTrackType).mInteger;
        const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideId{mObjectId};
        LLCC_SystemSetRide(rideId, track);
    }
    if ((mCellMode == 4 || mCellMode == 0x3b) && mF0C != 0) {
        LLCC_SystemSetRideQueue(info->Value(kRideSlot_HasQueue).mInteger != 0);
        LLCC_SystemSetRideTrack(info->Value(kRideSlot_WhichTrackType).mInteger);
    }
}

CMMFirstPersonTarget::~CMMFirstPersonTarget() {}
int32_t CMMFirstPersonTarget::GetMode() { return 9; }
CMMCoaster::~CMMCoaster() {}
int32_t CMMCoaster::GetMode() { return 8; }
CMMOnline::~CMMOnline() {}

int32_t CMMOnline::GetMode() { return 7; }

CMMPickUpStaff::~CMMPickUpStaff() {}

int32_t CMMPickUpStaff::GetMode() {
    return 6;
}

CMMPlaceStaff::~CMMPlaceStaff() {}

int32_t CMMPlaceStaff::GetMode() {
    return 5;
}

CMMDefault::~CMMDefault() {}
int32_t CMMDefault::GetMode() { return 1; }

CMMPathDraw::~CMMPathDraw() {}

int32_t CMMPathDraw::GetMode() { return 3; }

// 0x10081550: kills the script, if there is one.
void CSpritePlaceMode::OnRelease() {
    if (mScript != 0) SCR_KillScript(mScript);
    mScript = 0;
}
