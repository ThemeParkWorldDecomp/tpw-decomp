// Fakes and test entry points for unit w136, the game window's message functions (libtpw_unit_GameWin.dylib, driven by tests/test_GameWin.py;
// see calllog.h):
//   src/game/ui/ui_main.cpp  Engine_GameWinMsg 0x10183c84, Engine_GameFirstPersonMsg 0x10184628
// Every callee is a fake that records the same event as the PowerPC side's stub: the key tables, the pointer capture, the cursor, the mouse and
// its default mode, the cheat object, the camera, the context menu and the first-person functions. Answers come from per-case queues. The
// nothrow operator new answers from the queue "New" and records the Mac's sizes (1 for the mouse, 8 for its default mode).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Cheat.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "calllog.h"
#include "keyboard.h"
#include "ui_Postcard.h"
#include "ui_main.h"

using namespace calllog;

// ---- the globals the code reads and writes (the Mac addresses are in the Python module)
InterfaceWindow* gGameWindow;
InterfaceWindow* gFirstPersonWindow;
InterfaceWindow* gUIPanelWindow;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CPlayerSettings gPlayerSettings;
uint32_t gFirstPersonFlags;
int16_t gUIPointerX;
int16_t gUIPointerY;
int16_t gUIRealScreenWidth;
int16_t gUIRealScreenHeight;
CKeyTable* gKeyTableDebug;
CKeyTable* gKeyTableCamera;
CKeyTable* gKeyTableGame;
CKeyTable* gKeyTableShortcuts;
CKeyTable* gKeyTableCheat;

// Settings construction is outside the unit and has no event.
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

// ---- the test objects, labelled (see tpw_gw_obj)
namespace {
// A mouse mode whose answer is the queue "GetMode" (the original's is a virtual call through the vtable at +0x2c).
class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode", 0));
    }
    void OnAssign() override {}
};

alignas(16) unsigned char gWindowStore[3][sizeof(InterfaceWindow)];   // labels 1 to 3
CKeyTable gTables[5];                                                  // labels 4 camera, 5 cheat, 6 game, 7 shortcuts, 11 debug
alignas(16) unsigned char gMouseStore[sizeof(CMouse)];                 // label 8
FakeMode gFakeMode;                                                    // label 9
// label 10 is gGameWinCheat

void* Obj(int32_t label) {
    switch (label) {
        case 1: case 2: case 3: return gWindowStore[label - 1];
        case 4: case 5: case 6: case 7: return &gTables[label - 4];
        case 11: return &gTables[4];
        case 8: return gMouseStore;
        case 9: return &gFakeMode;
        case 10: return gGameWinCheat;
    }
    return nullptr;
}
}  // namespace

// ---- the nothrow new: the answer is the queue "New" (0 a NULL, the Mac's failed allocation). The size is recorded as the Mac's.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = static_cast<int64_t>(size);
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(CMMDefault)) mac = 8;   // the Mac's CMMDefault is a vtable pointer and one word; the host's is 16
    void* block = nullptr;
    if (ret("New", 1) != 0) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        newlab(block);
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}

// ---- the key tables: the answers come from the queues of the same names, one value per call
int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    ev("KeyDown", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("KeyDown", 0));
}
int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    ev("KeyUp", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("KeyUp", 0));
}
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("Search", 0));
}

// ---- the window
int32_t InterfaceWindow::CaptureDevice() {
    ev("CaptureDevice", {lab(this)});
    return 1;
}
int32_t InterfaceWindow::UnCaptureDevice() {
    ev("UnCaptureDevice", {lab(this)});
    return 1;
}
void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler", 0));
}

// ---- the clock and the modifier keys
uint32_t IveGotTicks() {
    ev("IveGotTicks");
    return static_cast<uint32_t>(ret("IveGotTicks", 0));
}
int16_t CheckShifts() {
    ev("CheckShifts");
    return static_cast<int16_t>(ret("CheckShifts", 0));
}

// ---- the mouse and its default mode
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
void CMouse::ImmediateMouseHandler(int32_t x, int32_t y) { ev("ImmediateMouseHandler", {lab(this), x, y}); }
void CMouse::ProcessIngameMouse(int32_t key, int32_t down, int32_t x, int32_t y, void* data) {
    ev("ProcessIngameMouse", {key, down, x, y, lab(data)});
}
CMMDefault::CMMDefault() { ev("CMMDefault.ctor", {lab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

// ---- the cheat object
CCheat::CCheat() { ev("CCheat.ctor", {lab(this)}); }
uint8_t CCheat::IsCheating() {
    ev("IsCheating", {lab(this)});
    return static_cast<uint8_t>(ret("IsCheating", 0));
}

// ---- the context menu, the camera, the rest
void ContextMouse(int32_t context) { ev("ContextMouse", {context}); }
int32_t ContextMouseClick() {
    ev("ContextMouseClick");
    return static_cast<int32_t>(ret("ContextMouseClick", 0));
}
void UnhighlightRide() { ev("UnhighlightRide"); }
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag}); }
void EndCoasterEditMode() { ev("EndCoasterEditMode"); }
void Will_HandleCameraMessages(int32_t message, int32_t a, int32_t b) { ev("Will_HandleCameraMessages", {message, a, b}); }
void StopRidingRideIn1stPerson() { ev("StopRidingRideIn1stPerson"); }
void ToggleFirstPersonWalkAround() { ev("ToggleFirstPersonWalkAround"); }
void UI_DoPostcard() { ev("UI_DoPostcard"); }

namespace {
constexpr int kStateWords = 26;
}

extern "C" {
// The address of the test object with this label (0 for none); the Python side labels it on the host.
void* tpw_gw_obj(int32_t label) { return Obj(label); }

// The state a case starts from (the order of STATE_KEYS in the Python module): the labels of gGameWindow, gFirstPersonWindow, gUIPanelWindow,
// gMouse and gMouseMode; the two mouse words; mRMBCancelOn; gFirstPersonFlags; the pointer x and y and the screen width and height; the
// statics (right down and its guard, press ticks and guard, camera on and guard, press x and y, pointer in, the two words, the cheat guard);
// the word 0x102c6cc8. The key tables are always set; the static CCheat starts as 0xa5 bytes.
void tpw_gw_state(const int32_t* in) {
    gGameWindow = static_cast<InterfaceWindow*>(Obj(in[0]));
    gFirstPersonWindow = static_cast<InterfaceWindow*>(Obj(in[1]));
    gUIPanelWindow = static_cast<InterfaceWindow*>(Obj(in[2]));
    gMouse = static_cast<CMouse*>(Obj(in[3]));
    gMouseMode = static_cast<CMouseMode*>(Obj(in[4]));
    gMouseWord1E0 = in[5];
    gMouseWord1E4 = in[6];
    gPlayerSettings.mCurrent.mRMBCancelOn = in[7] != 0;
    gFirstPersonFlags = static_cast<uint32_t>(in[8]);
    gUIPointerX = static_cast<int16_t>(in[9]);
    gUIPointerY = static_cast<int16_t>(in[10]);
    gUIRealScreenWidth = static_cast<int16_t>(in[11]);
    gUIRealScreenHeight = static_cast<int16_t>(in[12]);
    gGameWinRightDown = in[13];
    gGameWinRightDownGuard = static_cast<int8_t>(in[14]);
    gGameWinPressTicks = in[15];
    gGameWinPressTicksGuard = static_cast<int8_t>(in[16]);
    gGameWinCameraOn = in[17];
    gGameWinCameraOnGuard = static_cast<int8_t>(in[18]);
    gGameWinPressX = static_cast<int16_t>(in[19]);
    gGameWinPressY = static_cast<int16_t>(in[20]);
    gGameWinPointerIn = in[21];
    gGameWinWordC34 = in[22];
    gGameWinWordC38 = in[23];
    gGameWinCheatGuard = static_cast<int8_t>(in[24]);
    gRideHighlightKeep = in[25];
    std::memset(gGameWinCheat, 0xa5, sizeof gGameWinCheat);
    gKeyTableCamera = &gTables[0];
    gKeyTableCheat = &gTables[1];
    gKeyTableGame = &gTables[2];
    gKeyTableShortcuts = &gTables[3];
    gKeyTableDebug = &gTables[4];
}

// The state after a case, in the same order (labels for the five pointers), then the 12 bytes of the static CCheat.
void tpw_gw_dump(int32_t* out) {
    out[0] = lab(gGameWindow);
    out[1] = lab(gFirstPersonWindow);
    out[2] = lab(gUIPanelWindow);
    out[3] = lab(gMouse);
    out[4] = lab(gMouseMode);
    out[5] = gMouseWord1E0;
    out[6] = gMouseWord1E4;
    out[7] = gPlayerSettings.mCurrent.mRMBCancelOn ? 1 : 0;
    out[8] = static_cast<int32_t>(gFirstPersonFlags);
    out[9] = gUIPointerX;
    out[10] = gUIPointerY;
    out[11] = gUIRealScreenWidth;
    out[12] = gUIRealScreenHeight;
    out[13] = gGameWinRightDown;
    out[14] = gGameWinRightDownGuard;
    out[15] = gGameWinPressTicks;
    out[16] = gGameWinPressTicksGuard;
    out[17] = gGameWinCameraOn;
    out[18] = gGameWinCameraOnGuard;
    out[19] = gGameWinPressX;
    out[20] = gGameWinPressY;
    out[21] = gGameWinPointerIn;
    out[22] = gGameWinWordC34;
    out[23] = gGameWinWordC38;
    out[24] = gGameWinCheatGuard;
    out[25] = gRideHighlightKeep;
    for (int i = 0; i < 12; ++i) out[kStateWords + i] = gGameWinCheat[i];
}

int32_t tpw_gw_winmsg(int32_t window, int32_t message, int32_t a, int32_t b) {
    return Engine_GameWinMsg(static_cast<InterfaceWindow*>(Obj(window)), message, a, b);
}
int32_t tpw_gw_fpmsg(int32_t window, int32_t message, int32_t a, int32_t b) {
    return Engine_GameFirstPersonMsg(static_cast<InterfaceWindow*>(Obj(window)), message, a, b);
}
}
