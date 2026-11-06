// Test library for the main menu's whole life with the real bodies (libtpw_unit_MainMenuTeardown.dylib, tests/test_MainMenuTeardown.py):
//   src/game/ui/ui_mainmenu.cpp   HideMainMenu, MainGameMenuCB, MainLobbyMenuCB, MainMenuItemCB
//   src/game/ui/UIMenu.cpp        the menu's constructor, Init, AddItem, Show, Hide and destructor, the two default handlers
//   src/game/ui/InterfaceWindow.cpp, InterfaceTimer.cpp, InterfaceMessageQueue.cpp, UI_Base.cpp, CoolRect.cpp, CoolRegion.cpp,
//   InterfaceStringHeap.cpp: the window's life (constructor, destructor, message 4 and the default handler), the queues, the string heap
//
// Fakes that record: the draw objects (a window really owns and deletes them), the help text, the text measure, the font table, the
// colour and font setters, the menu handlers' other callees. The global operator new and delete are the real heap, so a window the menu
// no longer owns is really freed; run the module under libgmalloc to see a use after that.
#define TPW_UIMENU_CTOR_EXTERNAL 1
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "FillLenderList.h"
#include "Game.h"
#include "GameSys.h"
#include "InterfaceWindow.h"
#include "SaveLoad.h"
#include "TPString.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- the UI's globals the unit defines (the others live in files that are not linked)
InterfaceWindow* gUIPointerIn;
InterfaceWindow* gUICaptureDevice;
InterfaceWindow* gUIFocusWindow;
InterfaceWindow* gUIKeyboardWindow;
UIKeyCapture gUIKeyCapture;
InterfaceMessageQueue* gUIMessageQueue;
InterfaceMessageQueue* gUIDrawQueue;
InterfaceTimedMessages* gUITimedMessages;
InterfaceTimer* gUITimers;
int32_t gUITimerCount;
InterfaceTimer* gUITimerList;
InterfaceWindow* gUIButtonWindow[3];
InterfaceHelpText* gUIHelpText;
UIButtonState gUIButtons[3];
int32_t gUIDoubleClickTime;
int32_t gUIHelpTextResult;
int32_t (*gUIPeekCallBack)(InterfaceWindow*, int32_t, int32_t, int32_t);
int32_t gUIAutoRedraw;

extern uint8_t* gStringHeapTableA;
extern uint8_t* gStringHeapTableB;
extern int32_t gStringHeapRowsA;
extern int32_t gStringHeapRowsB;
extern int32_t gStringHeapWidthA;
extern int32_t gStringHeapWidthB;

UIMenu* gMainMenu;
CFSOnlineSystem* gOnlineSystem;
CFSUserInfo* gUserInfo;
CKeyTable* gKeyTableShortcuts;
CLBScreenSystem* gLobbyScreens;
int32_t gLobbyReturnFlag;
CWorld* gGameWorld;
OnlineUploadFile gOnlineUploadFile;
alignas(64) unsigned char gSaveLoadBytes[0x10000] __asm__("_gSaveLoad");
alignas(16) char gGameStorage[sizeof(CGame)] __asm__("_gGame");

namespace {
class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
};
class FakeHelp : public InterfaceHelpText {
public:
    void OnHideOrDelete(InterfaceWindow* window) override { ev("Help.OnHideOrDelete", {lab(this), lab(window)}); }
};
FakeDraw gLolight;
FakeHelp gHelp;
unsigned char gFont[16];
unsigned char gOnlineStore[16];
unsigned char gKeyTableStore[16];
constexpr int kEntries = 32;
InterfaceMessageQueue gQueues[2];
InterfaceMessage gMessages[2][kEntries];
alignas(InterfaceTimedMessages) unsigned char gTimedStore[sizeof(InterfaceTimedMessages)];
InterfaceTimedMessage gTimedEntries[4];

uint64_t U(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }
template <class T> T* P(uint64_t v) { return reinterpret_cast<T*>(static_cast<uintptr_t>(v)); }
}  // namespace

// ---- the window's untranslated members
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

// ---- the draw objects: a window owns the title text and borrows the library's lolight
InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    ev("Draw.Change", {lab(this), static_cast<int64_t>(flags), flags & 1 ? change->mFrame : 0, flags & 2 ? change->mDepth : 0});
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t shown) { ev("Draw.Show", {lab(this), shown}); }
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {std::strcmp(name, "lolight") == 0 ? 1 : 0});
    return &gLolight;
}

// ---- the help text: the one member the destructor calls records
InterfaceHelpText::InterfaceHelpText() {}
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t, int16_t) {}
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow*) {}
void InterfaceHelpText::OnMouseMovedOut() {}
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() {}
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow*) {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

// A timer starts free (its destructor is the unit's).
InterfaceTimer::InterfaceTimer() : mWindow(nullptr), mStart(0), mSlice(0), mId(0), mRunning(0), mFree(1), mNext(nullptr), mPrevious(nullptr) {}

// ---- the callees outside the unit, the same event name and arguments as the module's PowerPC stubs
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
int32_t UI_GetHighestZ(InterfaceWindow*, CoolRect*) {
    ev("UI_GetHighestZ");
    return 10;
}
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel");
    return false;
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture"); }
int32_t UIMain_UISound(int32_t code) {
    ev("UISound", {code});
    return 0;
}
void UI_MeasureText(const char16_t*, const TbRect& rect, int8_t font, int8_t, int8_t, int32_t, TbRect* out) {
    ev("MeasureText", {rect.right, rect.bottom, font});
    out->left = 0;
    out->top = 0;
    out->right = 99;
    out->bottom = 19;
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(gFont);
}
size_t wcslen(const char16_t* text) {
    size_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}
char16_t* wcscpy(char16_t* dst, const char16_t* src) {
    char16_t* d = dst;
    while ((*d++ = *src++) != 0) {}
    return dst;
}
// The arena on the plain allocator. InArena answers false, so InterfaceWindow's operator delete takes the logged global delete for every window and
// the whole tree's frees are in the event log and in reach of Guard Malloc; the real arena hides both.
namespace tpw {
uint8_t* gArenaBase = nullptr;
void* ArenaAlloc(size_t size) { return std::malloc(size); }
void* ArenaAllocZeroed(size_t size) { return std::calloc(1, size); }
void ArenaFree(void* block) { ::operator delete(block); }
bool InArena(const void*) { return false; }
}  // namespace tpw

namespace lender_callee {
void* OperatorNew(uint32_t size) { return ::operator new(size < 0x400 ? 0x400 : size); }
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    new (mem) InterfaceWindow(parent, attributes, id, left, top, right, bottom);
}
}  // namespace lender_callee
namespace menu_callee {
void RectCtor(void* mem) { new (mem) CoolRect(); }
void RectDtor(void* mem) { static_cast<CoolRect*>(mem)->~CoolRect(); }
void TitleTextCtor(void* mem) {
    new (mem) FakeDraw();
    ev("TitleCtor", {newlab(mem)});
}
void WindowSetText(InterfaceWindow* window, char16_t*) { ev("SetText", {lab(window)}); }
}  // namespace menu_callee

int32_t msl_rand() { return 7; }

// The window kinds CreateObject can make: a menu item is kind 1 only.
extern "C" void tpw_mt_bar() __asm__("__ZN12InterfaceBarC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_bar() {}
extern "C" void tpw_mt_knob() __asm__("__ZN13InterfaceKnobC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_knob() {}
extern "C" void tpw_mt_image() __asm__("__ZN14InterfaceImageC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_image() {}
extern "C" void tpw_mt_button() __asm__("__ZN15InterfaceButtonC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_button() {}
extern "C" void tpw_mt_edit() __asm__("__ZN16InterfaceEditBoxC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_edit() {}
extern "C" void tpw_mt_list() __asm__("__ZN16InterfaceListBoxC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_list() {}
extern "C" void tpw_mt_text() __asm__("__ZN16InterfaceTextBoxC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_text() {}
extern "C" void tpw_mt_vary() __asm__("__ZN16InterfaceVaryBoxC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_vary() {}
extern "C" void tpw_mt_scroll() __asm__("__ZN18InterfaceScrollBarC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_scroll() {}
extern "C" void tpw_mt_light() __asm__("__ZN19InterfaceLightPanelC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_light() {}
extern "C" void tpw_mt_check() __asm__("__ZN21InterfaceCheckButtonsC1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_check() {}
extern "C" void tpw_mt_control() __asm__("__ZN21InterfaceListControl2C1EP15InterfaceWindowiissss");
extern "C" void tpw_mt_control() {}

// The dialogs' answers the handlers name: identities only.
int32_t MainGameMenuConfirmQuit_OK(void*) { return 13; }
int32_t MainLobbyMenuConfirmQuit_OK(void*) { return 14; }
int32_t MainGameMenuConfirmPublish_OK(void*) { return 15; }
int32_t MainGameMenuConfirmRestart_OK(void*) { return 11; }
int32_t MainLobbyMenuConfirmLogout_OK(void*) { return 16; }
int32_t BankruptGameMenuConfirmRestart_OK(void*) { return 12; }
void MainLobbyMenuPostOnlineLoginCallbackFn(int32_t, const void*) {}

// What the default handler reaches on messages the module does not send.
uint32_t IveGotTicks() { return 0; }
void UI_SetCursor(int16_t) {}
void InterfaceTimer::Stop() {}
InterfaceMessageQueue::InterfaceMessageQueue() {}
InterfaceMessageQueue::~InterfaceMessageQueue() {}
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing"); }
void CLBScreenComponentAdvisor::Add(AdvisorResponse, int32_t) {}
namespace mainmenu_callee {
void ScreenShow(void*) {}
void ScreenHide(void*) {}
int32_t ScreenIsVisible(void*) { return 0; }
void PublishGlobeSlot34(void*) {}
CFSOnlineSystem* NewOnlineSystem() { return nullptr; }
}  // namespace mainmenu_callee

// The menu handlers' other callees.
void CGame::Unpause() { ev("Unpause", {lab(this)}); }
void UI_UnPauseTimerSystem() { ev("UI_UnPauseTimerSystem"); }
void CreateLoadPanel(int32_t mode) { ev("CreateLoadPanel", {mode}); }
void CreateSavePanel(int32_t mode) { ev("CreateSavePanel", {mode}); }
void CreateOptionsPanel() { ev("CreateOptionsPanel"); }
void UI_ByeByeBankrupt() { ev("UI_ByeByeBankrupt"); }
void CreateLoginPanel() { ev("CreateLoginPanel"); }
void GS_SetCurrentGameStatus(GS_GAME_STATES status) { ev("GS_SetCurrentGameStatus", {static_cast<int32_t>(status)}); }
const char16_t* UIMain_GetString(int32_t) {
    static const char16_t text[] = {0x53, 0};
    return text;
}
InterfaceWindow* UI_DialogBox(dlgboxparams&, dlgbutton, dlgcallback, dlgbutton, dlgcallback, dlgbutton, dlgcallback, bool) {
    ev("UI_DialogBox");
    return nullptr;
}
int32_t CFSOnlineSystem::AreWeProcessing() {
    ev("AreWeProcessing");
    return 0;
}
void CFSOnlineSystem::Logout() { ev("Logout"); }
void CFSOnlineSystem::Login(void (*)(int32_t, const void*), const void*) { ev("Login"); }
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {key, modifiers});
    return static_cast<int32_t>(ret("Search"));
}

extern "C" {

uint64_t tpw_mt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"pointer_in", &gUIPointerIn}, {"capture", &gUICaptureDevice}, {"focus", &gUIFocusWindow}, {"keyboard", &gUIKeyboardWindow},
        {"key_keys", &gUIKeyCapture.mKeys}, {"key_window", &gUIKeyCapture.mWindow}, {"last_window", &gUILastWindow},
        {"button0", &gUIButtonWindow[0]}, {"button1", &gUIButtonWindow[1]}, {"button2", &gUIButtonWindow[2]},
        {"root", &gUIRootWindow}, {"menu", &gMainMenu}, {"flag", &gMainMenuPauseFlag}, {"gGame", gGameStorage},
        {"sys", &gOnlineSystem}, {"keytable", &gKeyTableShortcuts}, {"width", &gUIRealScreenWidth}, {"height", &gUIRealScreenHeight},
        {"TableB", &gStringHeapTableB}, {"TableA", &gStringHeapTableA}, {"RowsB", &gStringHeapRowsB}, {"WidthB", &gStringHeapWidthB},
        {"RowsA", &gStringHeapRowsA}, {"WidthA", &gStringHeapWidthA}, {"queue", &gQueues[0]}, {"draw_queue", &gQueues[1]},
        {"retflag", &gLobbyReturnFlag},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return U(entry.address);
    return 0;
}

// The offsets the module reads the host's objects with.
int32_t tpw_mt_layout(int64_t* out) {
    int32_t n = 0;
#define W(field) out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, field))
    W(mDestroying); W(mAttributes); W(mNext); W(mPrevious); W(mChild); W(mParent); W(mDrawObject); W(mClientDrawObject);
    W(mExtraParams); W(mZ); W(mId); W(mRect);
#undef W
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mF04));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mShown));
    out[n++] = static_cast<int64_t>(offsetof(UIMenu, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceMessageQueue, mCount));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceMessageQueue, mHead));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceMessageQueue, mTail));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceMessageQueue, mQueued));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceMessage));
    out[n++] = static_cast<int64_t>(offsetof(CGame, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(CoolRect, mLeft));
    return n;
}

uint64_t tpw_mt_alloc(uint32_t size) { return U(tpw::ArenaAllocZeroed(size)); }
uint64_t tpw_mt_entries(int32_t which) { return U(gMessages[which]); }

// The UI's start-up state for a case: a root window, empty queues, nothing captured. The lolight and the font have labels in the case.
void tpw_mt_reset() {
    gUIPointerIn = gUICaptureDevice = gUIFocusWindow = gUIKeyboardWindow = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    gUIKeyCapture.mWindow = nullptr;
    for (InterfaceWindow*& window : gUIButtonWindow) window = nullptr;
    gUITimers = nullptr;
    gUITimerCount = 0;
    gUITimerList = nullptr;
    gUIHelpText = &gHelp;
    gUIPeekCallBack = nullptr;
    gUIAutoRedraw = 0;
    gUILastWindow = nullptr;
    gMainMenu = nullptr;
    gLobbyReturnFlag = 0;
    gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(gOnlineStore);
    gKeyTableShortcuts = reinterpret_cast<CKeyTable*>(gKeyTableStore);
    std::memset(gMessages, 0, sizeof gMessages);
    for (int k = 0; k < 2; ++k) {
        gQueues[k].mEntries = gMessages[k];
        gQueues[k].mCount = kEntries;
        gQueues[k].mHead = kEntries - 1;
        gQueues[k].mTail = kEntries - 1;
        gQueues[k].mQueued = 0;
    }
    gUIMessageQueue = &gQueues[0];
    gUIDrawQueue = &gQueues[1];
    std::memset(gTimedStore, 0, sizeof gTimedStore);
    std::memset(gTimedEntries, 0, sizeof gTimedEntries);
    auto* timed = reinterpret_cast<InterfaceTimedMessages*>(gTimedStore);
    timed->mCount = 4;
    timed->mEntries = gTimedEntries;
    gUITimedMessages = timed;
    gLolight.mInLibrary = 1;
    gUIRootWindow = new InterfaceWindow(nullptr, 1, 0, 0, 0, static_cast<int16_t>(0x7ff), static_cast<int16_t>(0x5ff));
}

uint64_t tpw_mt_fixed(int32_t which) {
    switch (which) {
    case 0: return U(gLolight.InstanceMe());
    case 1: return U(gFont);
    case 2: return U(&gHelp);
    case 3: return U(gOnlineStore);
    case 4: return U(gKeyTableStore);
    }
    return 0;
}

uint64_t tpw_mt_new_menu() { return U(new UIMenu()); }
uint64_t tpw_mt_new_window(uint64_t parent, int32_t attributes, int32_t id, int32_t left, int32_t top, int32_t right, int32_t bottom) {
    return U(new InterfaceWindow(P<InterfaceWindow>(parent), attributes, id, static_cast<int16_t>(left), static_cast<int16_t>(top),
                                 static_cast<int16_t>(right), static_cast<int16_t>(bottom)));
}
void tpw_mt_init(uint64_t menu, int32_t lobby) { P<UIMenu>(menu)->Init(lobby != 0 ? MainLobbyMenuCB : MainGameMenuCB, MainMenuItemCB); }
int32_t tpw_mt_add(uint64_t menu, int32_t position, uint64_t text, int32_t id, int32_t a, int32_t b, int32_t c, int32_t d) {
    return P<UIMenu>(menu)->AddItem(position, P<char16_t>(text), id, a, b, c, d);
}
void tpw_mt_show(uint64_t menu, int32_t a) { P<UIMenu>(menu)->Show(a); }
void tpw_mt_hide() { HideMainMenu(); }
void tpw_mt_destruct(uint64_t menu, int32_t flag) { UIMenuDestruct(P<UIMenu>(menu), static_cast<int16_t>(flag)); }
int32_t tpw_mt_post(uint64_t window, int32_t message, int32_t a, int32_t b) { return UI_PostMessage(P<InterfaceWindow>(window), message, a, b); }
int32_t tpw_mt_pump(int32_t which) { return gQueues[which].Post(); }
void tpw_mt_link(uint64_t window) { gUIRootWindow->link(P<InterfaceWindow>(window), 1, 1); }

}  // extern "C"
