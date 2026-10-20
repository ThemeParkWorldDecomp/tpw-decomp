// Test library for unit w49 (libtpw_unit_UIKbCapture.dylib; see tests/test_UIKbCapture.py): src/ui_main_kbcapture.cpp
// (UIMain_DecideKbCapture, MainMenuUp, UIMenu::IsShown) and src/ui_coasteredit_kb.cpp (CoasterEditPanelUp, CoasterEditPanelGetKb), with
// the real key table Enable and the window's CaptureKb (GetAttr is below). The only fake is the message poster that CaptureKb calls (it records);
// the windows are ten blocks the size of an InterfaceWindow that are never constructed (only the attribute word is read), the three key
// tables the decision enables are real objects, and the menu and the lobby are blocks of their size. The Python module drives it through
// tpw_kb_run and reads the state back.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"

#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "platform/msl.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_menu.h"

using namespace calllog;

namespace {
constexpr int kWindows = 10;

alignas(16) unsigned char gWinStorage[kWindows][sizeof(InterfaceWindow)];
alignas(16) unsigned char gLobbyStorage[sizeof(CLBLobby)];
UIMenu gMenu;
CKeyTable gTables[3];   // camera, game, cheat

InterfaceWindow* Win(int i) { return reinterpret_cast<InterfaceWindow*>(gWinStorage[i]); }
InterfaceWindow* Pick(int index) { return index < 0 ? nullptr : Win(index); }

// The window pointer as an index (-1 for NULL, -2 for any other pointer).
int WindowIndex(const void* window) {
    for (int i = 0; i < kWindows; ++i)
        if (window == Win(i)) return i;
    return window == nullptr ? -1 : -2;
}
}  // namespace

// ---- globals of other files
InterfaceWindow* gUIKeyboardWindow;
CLBLobby* gLobby;
CKeyTable* gKeyTable;
CKeyTable* gKeyTableGame;
CKeyTable* gKeyTableCamera;
CKeyTable* gKeyTableCheat;
CKeyTable* gKeyTableCoaster;
CKeyTable* gKeyTableShortcuts;
CKeyTable* gKeyTableDebug;

// ---- the message poster CaptureKb calls: records its arguments, and answers 0
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}

// ---- InterfaceWindow_script.cpp is linked for CaptureKb alone; the window script's callees are never reached here
void msl_assertion_failed(const char*, const char*, int) {}
int32_t UI_DefaultThumbtrackCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
CoolCircle::CoolCircle(int16_t, int16_t, int16_t) {}
CoolEllipse::CoolEllipse(int16_t, int16_t, int16_t, int16_t) {}
CoolPoly::CoolPoly(int16_t, const int16_t*) {}
void InterfaceKnob::SetMinMaxAngles(int32_t*, int32_t*) {}
InterfaceWindow* InterfaceWindow::CreateObject(int32_t, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) { return nullptr; }
void InterfaceWindow::MakeRectAbsolute(CoolRect*) {}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t) { return nullptr; }
void InterfaceListBox::SetListRect(int16_t, int16_t, int16_t, int16_t) {}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t) { return nullptr; }
void InterfaceListControl2::SetColumns(int16_t) {}
void InterfaceListControl2::SetListRect(int16_t, int16_t, int16_t, int16_t) {}
void InterfaceListControl2::SetColumnPos(int16_t, int16_t, int16_t) {}

// ---- GetAttr is one line in UI_pump.cpp, which is the whole message pump; here it is the same line
uint32_t InterfaceWindow::GetAttr() { return mAttributes; }

// ---- the driver
extern "C" {
void* tpw_kb_window(int i) { return Win(i); }
void* tpw_kb_table(int i) { return &gTables[i]; }
void* tpw_kb_menu() { return &gMenu; }
void* tpw_kb_lobby() { return gLobbyStorage; }

// Makes the world: ten windows with attribute words 0, all the globals NULL, the tables' enabled words 0.
void tpw_kb_reset() {
    std::memset(gWinStorage, 0, sizeof gWinStorage);
    std::memset(gLobbyStorage, 0, sizeof gLobbyStorage);
    std::memset(&gMenu, 0, sizeof gMenu);
    std::memset(gTables, 0, sizeof gTables);
    gGameWindow = gFirstPersonWindow = gBumholeWindow = gOnlinePanel = gOnlineWindow = nullptr;
    gBankruptWindow = gUIPanelWindow = gCoasterPanel = nullptr;
    gMainMenu = nullptr;
    gCoasterEditMode = 0;
    gLobby = nullptr;
    gUIKeyboardWindow = nullptr;
    gKeyTableCamera = &gTables[0];
    gKeyTableGame = &gTables[1];
    gKeyTableCheat = &gTables[2];
}

// Window i's attribute word.
void tpw_kb_setattr(int i, uint32_t attributes) { Win(i)->mAttributes = attributes; }

// The window globals (window indexes, -1 for NULL): the game window, the first-person window, the bumhole window, the online panel, the
// online window, the bankrupt window, the panel and the coaster panel; and the coaster editor's flag.
void tpw_kb_windows(const int32_t* index, int32_t coasterFlag) {
    gGameWindow = Pick(index[0]);
    gFirstPersonWindow = Pick(index[1]);
    gBumholeWindow = Pick(index[2]);
    gOnlinePanel = Pick(index[3]);
    gOnlineWindow = Pick(index[4]);
    gBankruptWindow = Pick(index[5]);
    gUIPanelWindow = Pick(index[6]);
    gCoasterPanel = Pick(index[7]);
    gCoasterEditMode = coasterFlag;
}

// The menu: present or not, the word IsShown answers, and its window (an index); the lobby: 0 none, 1 with no window, 2 with the window
// `lobbyWindow`; the window that has the keyboard before the call (an index, -1 none).
void tpw_kb_menu_lobby(int present, int32_t shown, int menuWindow, int lobby, int lobbyWindow, int keyboard) {
    gMenu.mShown = shown;
    gMenu.mWindow = Pick(menuWindow);
    gMainMenu = present != 0 ? &gMenu : nullptr;
    reinterpret_cast<CLBLobby*>(gLobbyStorage)->mWindow = lobby == 2 ? Pick(lobbyWindow) : nullptr;
    gLobby = lobby != 0 ? reinterpret_cast<CLBLobby*>(gLobbyStorage) : nullptr;
    gUIKeyboardWindow = Pick(keyboard);
}

// The three tables' enabled words before the call.
void tpw_kb_enabled(const int16_t* words) {
    for (int i = 0; i < 3; ++i) gTables[i].mEnabled = words[i];
}

// The state after a call: the window that has the keyboard (an index), the three tables' enabled words.
void tpw_kb_state(int32_t* out) {
    out[0] = WindowIndex(gUIKeyboardWindow);
    for (int i = 0; i < 3; ++i) out[1 + i] = gTables[i].mEnabled;
}

// Runs one of the functions. op 0: UIMain_DecideKbCapture. 1: MainMenuUp (out[0]). 2: CoasterEditPanelUp (out[0]). 3: CoasterEditPanelGetKb.
// 4: UIMenu::IsShown of the menu (out[0]).
void tpw_kb_run(int op, int32_t* out) {
    out[0] = 0;
    switch (op) {
    case 0: UIMain_DecideKbCapture(); break;
    case 1: out[0] = MainMenuUp(); break;
    case 2: out[0] = CoasterEditPanelUp(); break;
    case 3: CoasterEditPanelGetKb(); break;
    case 4: out[0] = gMenu.IsShown(); break;
    }
}
}
