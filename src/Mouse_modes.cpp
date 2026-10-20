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
#include <cstdint>
#include <new>

#include "Debug.h"
#include "FS_Online.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "online.h"

// __sinit_Mouse_cpp (0x100815c8) stores these two words on the Mac; the host initialises them at load time. The names are invented.
int32_t gMouseWord1F8 = 0x10000;   // 0x102dc1f8
int32_t gMouseWord1FC = 0;         // 0x102dc1fc

// The null ids (const halfwords of the image, both 0; the names are invented): the pointer's "no cell" id and LLCR_OverRide's "no ride" answer.
extern const uint16_t gPointerNullId = 0;    // 0x1029d432
extern const uint16_t gOverRideNullId = 0;   // 0x1029d436

namespace {
constexpr const char* kAssigningOnline = "Assigning CMMOnline\n";   // 0x1023e03c
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023dff9

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

// The Mac makes the mouse and its default mode in this order inline: the mouse (an empty class on the Mac, operator new(1)), then the default
// mode (operator new(8)), which the mouse takes; then the two words of the mouse's data are set to -1. A mouse that could not be made leaves
// the words alone. The answer is the mouse, NULL when it could not be made. (The same idiom as src/Mouse_staff.cpp's, written again here.)
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

CMMPathDraw::~CMMPathDraw() {}

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

int32_t CMMPathDraw::GetMode() { return 3; }

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

// ---- CMMOnline (vtable 0x1029d4ec, 8 bytes)
CMMOnline::CMMOnline() {
    mWord4 = 0;
    gUIInterface.SetGameWindowCursor(0);
}

CMMOnline::~CMMOnline() {}

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

int32_t CMMOnline::GetMode() { return 7; }
void CMMOnline::OnAssign() { CTRACE(3, kAssigningOnline); }

// ---- CMMCoaster (vtable 0x1029d4a0, 8 bytes)
CMMCoaster::CMMCoaster() { mWord4 = 0; }
CMMCoaster::~CMMCoaster() {}
void CMMCoaster::OnLeftClick(int32_t, int32_t) { CoasterAcceptLeftMouseClick(1); }
void CMMCoaster::OnLeftRelease(int32_t, int32_t) { CoasterAcceptLeftMouseClick(0); }
void CMMCoaster::OnRightClick(int32_t, int32_t) {}
int32_t CMMCoaster::GetMode() { return 8; }
void CMMCoaster::OnAssign() {}

// ---- CMMFirstPersonTarget (vtable 0x1029d454, 8 bytes)
CMMFirstPersonTarget::CMMFirstPersonTarget() { mWord4 = 0; }
CMMFirstPersonTarget::~CMMFirstPersonTarget() {}
void CMMFirstPersonTarget::OnRightClick(int32_t, int32_t) {}
int32_t CMMFirstPersonTarget::GetMode() { return 9; }
void CMMFirstPersonTarget::OnAssign() { gUIInterface.SetGameWindowCursor(kCursorTarget); }

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
