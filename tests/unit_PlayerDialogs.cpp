// Test library for unit w47 (libtpw_unit_PlayerDialogs.dylib; see tests/test_PlayerDialogs.py): src/game/ui/ui_playerLogin.cpp (CreatePlayerMsg,
// NewNameOK, GameConfirmQuit_OK, GameConfirmDelPlayer_OK), src/game/ui/InterfaceTimer.cpp, src/game/ui/ui_ErrorDisplay.cpp, src/game/ui/ui_main.cpp (DialogBoxCallback, DefaultDialogCallback,
// UIMain_EnableAllKeyTables, UI_UnPauseTimerSystem) and src/game/ui/InterfaceButton.cpp (GetCurrent), with the real string class and
// wcslen.
//
// Everything else the functions call is a fake that records through calllog: the window's GetWindow, Show, getExtraParam, GetParent and
// default handler, the message senders, the key tables, the clock, the pause, the user information (constructor, create, login, delete), the
// lobby's SendQuit, the keyboard decision, the panel's removal and name filling. The windows are six blocks the size of the check
// buttons (never constructed; only their word at 0x138 is read), the user information and the lobby are blocks of their size, and the two
// dialog callbacks are fakes whose answers come from a queue. The Python module drives it through tpw_pd_run and reads the state back.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"

#include "FS_UserInfo.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

namespace {
constexpr int kWindows = 6;
constexpr int kExtras = 6;
constexpr int kTimers = 4;
constexpr int kNameUnits = 18;

// (The labels of the world's fixed objects are the Python module's: windows 10 to 15, the user information 20, the lobby 21, the game 22.)
alignas(16) unsigned char gWinStorage[kWindows][sizeof(InterfaceCheckButtons)];
alignas(16) unsigned char gUserStorage[sizeof(CFSUserInfo)];
alignas(16) unsigned char gLobbyStorage[sizeof(CLBLobby)];
alignas(16) unsigned char gTimerStorage[kTimers][sizeof(InterfaceTimer)];

InterfaceCheckButtons* Win(int i) { return reinterpret_cast<InterfaceCheckButtons*>(gWinStorage[i]); }
InterfaceTimer* Timer(int i) { return reinterpret_cast<InterfaceTimer*>(gTimerStorage[i]); }
int gParent[kWindows];                // a window's parent as an index, -1 for none
void* gExtra[kWindows][kExtras];      // a window's extra parameters

// The window pointer as an index (-1 for NULL, -2 for any other pointer).
int WindowIndex(const void* window) {
    for (int i = 0; i < kWindows; ++i)
        if (window == Win(i)) return i;
    return window == nullptr ? -1 : -2;
}

int32_t Low32(const void* pointer) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(pointer)); }

// The two callbacks the dialog's buttons can have: they record the argument's low 32 bits and answer the queue's value.
int32_t CallbackFirst(void* data) {
    ev("CbFirst", {Low32(data)});
    return static_cast<int32_t>(ret("CbFirst"));
}
int32_t CallbackSecond(void* data) {
    ev("CbSecond", {Low32(data)});
    return static_cast<int32_t>(ret("CbSecond"));
}
}  // namespace

// ---- the string class's Format (a vtable slot nothing here calls; the rest of the class is real)
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files
InterfaceWindow* gNewNameWindow;
char16_t gNewNameText[18];
int32_t gLoginPanelFirstPlayer;
CFSUserInfo* gUserInfo;
CLBLobby* gLobby;
CGame gGame;
uint8_t gUIMainKeyTablesDisabled;
int32_t gUITimersPaused;
InterfaceTimer* gUITimerList;

// ---- the window's members the unit's functions use. Each one records its arguments.
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    const int64_t index = ret("GetWindow", id, 0);
    return index >= 1 && index <= kWindows ? Win(static_cast<int>(index) - 1) : nullptr;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    const int window = WindowIndex(this);
    return window >= 0 && index >= 0 && index < kExtras ? gExtra[window][index] : nullptr;
}
InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {lab(this)});
    const int window = WindowIndex(this);
    return window >= 0 && gParent[window] >= 0 ? Win(gParent[window]) : nullptr;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}
// The check buttons' SetState (SetCurrent in the same file calls it; nothing here does).
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("Button.SetState", {lab(this), state, redraw});
    return 1;
}

// ---- messages, keyboard, key tables, clock, pause
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UI_SendMessage"));
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UI_PostMessage"));
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture"); }
void EnableAllKeytables() { ev("EnableAllKeytables"); }
// The real one (src/game/ui/ui_main.cpp) disables the tables and sets the bookkeeping byte.
void UIMain_DisableAllKeyTables() {
    ev("UIMain_DisableAllKeyTables");
    gUIMainKeyTablesDisabled = 1;
}
uint32_t IveGotTicks() {
    ev("IveGotTicks");
    return static_cast<uint32_t>(ret("IveGotTicks"));
}
void CGame::Unpause() { ev("CGame.Unpause", {lab(this)}); }
CGame::CGame() {}
CGame::~CGame() {}

// ---- the login panel, the user information and the lobby
void RemoveLoginPanel(int32_t cancel) { ev("RemoveLoginPanel", {cancel}); }
void FillLoginNames() { ev("FillLoginNames"); }
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
// The members' constructors have made the four slot names' buffers by now (new[]); the original's constructor is a fake here, so they are
// taken off the heap count that the test compares.
CFSUserInfo::CFSUserInfo() {
    tpw_cl_heap[1] -= 4;
    ev("UserInfo.ctor", {newlab(this)});
}
// The queue "ClearUser" makes the call destroy the program's user information (the original makes it again before the login).
void CFSUserInfo::CreateNewGameLoginUserInSlot(EFSUserInfoGameUser slot, const TbDynamicStringTemplate<char16_t>& name, int32_t easyMode) {
    const std::string text = narrow(name.mData);
    ev("UserInfo.CreateNew", {lab(this), static_cast<int32_t>(slot), easyMode}, {text.c_str()});
    if (ret("ClearUser") != 0) gUserInfo = nullptr;
}
void CFSUserInfo::GameLogin(EFSUserInfoGameUser slot) { ev("UserInfo.GameLogin", {lab(this), static_cast<int32_t>(slot)}); }
void CFSUserInfo::DeleteGameLoginUserInSlot(EFSUserInfoGameUser slot) {
    ev("UserInfo.Delete", {lab(this), static_cast<int32_t>(slot)});
}
void CLBLobby::SendQuit() { ev("SendQuit", {lab(this)}); }

// ---- the driver
extern "C" {
void* tpw_pd_window(int i) { return Win(i); }
void* tpw_pd_user() { return gUserStorage; }
void* tpw_pd_lobby() { return gLobbyStorage; }
void* tpw_pd_game() { return &gGame; }

// Makes the world: the windows with no parent, no extra parameters and a current id of -1, everything else cleared.
void tpw_pd_reset() {
    std::memset(gWinStorage, 0, sizeof gWinStorage);
    std::memset(gUserStorage, 0, sizeof gUserStorage);
    std::memset(gLobbyStorage, 0, sizeof gLobbyStorage);
    std::memset(gTimerStorage, 0, sizeof gTimerStorage);
    std::memset(gExtra, 0, sizeof gExtra);
    std::memset(gNewNameText, 0, sizeof gNewNameText);
    for (int i = 0; i < kWindows; ++i) {
        gParent[i] = -1;
        Win(i)->mCurrent = -1;
    }
    gNewNameWindow = nullptr;
    gLoginPanelFirstPlayer = 0;
    gUserInfo = nullptr;
    gLobby = nullptr;
    gGame.mF3C = 0;
    gUIMainKeyTablesDisabled = 0;
    gUITimersPaused = 0;
    gUITimerList = nullptr;
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}

// Window i: the check buttons' current id (the word at 0x138) and its parent (an index, -1 for none).
void tpw_pd_setwin(int i, int32_t current, int parent) {
    Win(i)->mCurrent = current;
    gParent[i] = parent;
}

// Extra parameter `slot` of window i: kind 0 is the number `value` (as a pointer), 1 the first fake callback, 2 the second.
void tpw_pd_setextra(int i, int slot, int kind, int32_t value) {
    if (kind == 1) gExtra[i][slot] = reinterpret_cast<void*>(CallbackFirst);
    else if (kind == 2) gExtra[i][slot] = reinterpret_cast<void*>(CallbackSecond);
    else gExtra[i][slot] = reinterpret_cast<void*>(static_cast<intptr_t>(value));
}

// The name buffer (18 characters).
void tpw_pd_setname(const uint16_t* chars) {
    for (int i = 0; i < kNameUnits; ++i) gNewNameText[i] = chars[i];
}

// The globals: the name entry dialog (a window index, -1 for none), whether the user information and the lobby exist, the game's word at
// 0x3c, the timers' pause word and the key tables' bookkeeping byte, and the first player flag.
void tpw_pd_globals(int dialog, int user, int lobby, int32_t gameWord, int32_t paused, int keyByte, int32_t first) {
    gNewNameWindow = dialog < 0 ? nullptr : Win(dialog);
    gUserInfo = user != 0 ? reinterpret_cast<CFSUserInfo*>(gUserStorage) : nullptr;
    gLobby = lobby != 0 ? reinterpret_cast<CLBLobby*>(gLobbyStorage) : nullptr;
    gGame.mF3C = gameWord;
    gUITimersPaused = paused;
    gUIMainKeyTablesDisabled = static_cast<uint8_t>(keyByte);
    gLoginPanelFirstPlayer = first;
}

// The timers: four with the start ticks given; the first `count` are the list.
void tpw_pd_timers(int count, const uint32_t* starts) {
    for (int i = 0; i < kTimers; ++i) {
        Timer(i)->mStart = starts[i];
        Timer(i)->mNext = i + 1 < count ? Timer(i + 1) : nullptr;
    }
    gUITimerList = count > 0 ? Timer(0) : nullptr;
}

// The state after a call, in the order of the Python module's dump: the dialog's index, the first player flag, the label of the user
// information, the key byte, the pause word, the game's word, the four start ticks, the name buffer.
void tpw_pd_state(int32_t* out) {
    int n = 0;
    out[n++] = WindowIndex(gNewNameWindow);
    out[n++] = gLoginPanelFirstPlayer;
    out[n++] = lab(gUserInfo);
    out[n++] = gUIMainKeyTablesDisabled;
    out[n++] = gUITimersPaused;
    out[n++] = gGame.mF3C;
    for (int i = 0; i < kTimers; ++i) out[n++] = static_cast<int32_t>(Timer(i)->mStart);
    for (int i = 0; i < kNameUnits; ++i) out[n++] = gNewNameText[i];
}

// Runs one of the functions. op 0: CreatePlayerMsg(window p0, message p1, a p2, b p3). 1: NewNameOK(slot p1). 2: DefaultDialogCallback
// (as 0). 3: DialogBoxCallback (as 0). 4: UIMain_EnableAllKeyTables. 5: UI_UnPauseTimerSystem. 6: GameConfirmQuit_OK(p1). 7:
// GameConfirmDelPlayer_OK(slot p1). 8: GetCurrent of window p0. out[0] is the answer.
void tpw_pd_run(int op, int p0, int32_t p1, int32_t p2, int32_t p3, int32_t* out) {
    InterfaceWindow* window = p0 >= 0 ? Win(p0) : nullptr;
    void* argument = reinterpret_cast<void*>(static_cast<intptr_t>(p1));
    out[0] = 0;
    switch (op) {
    case 0: out[0] = CreatePlayerMsg(window, p1, p2, p3); break;
    case 1: out[0] = NewNameOK(argument); break;
    case 2: out[0] = DefaultDialogCallback(window, p1, p2, p3); break;
    case 3: out[0] = DialogBoxCallback(window, p1, p2, p3); break;
    case 4: UIMain_EnableAllKeyTables(); break;
    case 5: UI_UnPauseTimerSystem(); break;
    case 6: out[0] = GameConfirmQuit_OK(argument); break;
    case 7: out[0] = GameConfirmDelPlayer_OK(argument); break;
    case 8: out[0] = static_cast<InterfaceCheckButtons*>(window)->GetCurrent(); break;
    }
}
}
