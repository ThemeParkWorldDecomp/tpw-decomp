// Test library for the online panel builder and the park find panel's message function (unit v5541;
// libtpw_unit_OnlineParkFindPanelMsg.dylib, tests/test_OnlineParkFindPanelMsg.py), main app:
//   src/OnlineParkFindPanelMsg.cpp   CreateOnlinePanel, OnlineParkFindPanelMsg and the globals they own
//
// Every callee outside the unit is a recording fake with the same event name and arguments as the module's PowerPC stub. The window
// objects are 40 Probe windows (an InterfaceWindow whose virtual SetText records); constructors of classes whose bases are not linked
// (CoolRect, CFSOnlineSystem, the cursor position text sprite) are defined under their mangled complete-object names.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_tooltip.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem;
CLBLobby* gLobby;
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gOnlineWindowE8c;
InterfaceWindow* gOnlineWindowE90;
int32_t gOnlineContactsBusy;
int32_t gOnlineCursorState;
uint8_t gOnlinePanelShown;
char16_t gOnlineEmptyHelp[2] = {0, 0x2173};
float gGameWindowCentreX;
float gGameWindowCentreY;
float gGameWindowHalfWidth = 1.0f;
float gGameWindowHalfHeight = 1.0f;
float gScreenCentreX;
float gScreenCentreY;
float gViewCentreX;
float gViewCentreY;

namespace {

volatile int32_t gDistinct;   // keeps the empty fakes from being merged into one address
#define DISTINCT() (gDistinct = gDistinct + __LINE__)

template <class F>
int64_t lf(F f) {
    return lab(reinterpret_cast<const void*>(f));
}

char gDrawDummy[64];
char gFontDummy[64];
alignas(16) char gListBuf[64];
alignas(16) char gSysBuf[sizeof(CFSOnlineSystem)];
alignas(16) char gLobbyBuf[sizeof(CLBLobby)];

struct Probe : InterfaceWindow {
    Probe() : InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0) {}
    int32_t SetText(char16_t* text) override {
        ev("SetText", {lab(this), lab(text)});
        return 1;
    }
};
Probe* gObj[40];

void MakeObjects() {
    if (gObj[0] != nullptr) return;
    for (int i = 0; i < 40; ++i) gObj[i] = new Probe();
}

InterfaceWindow* ObjOf(int64_t index) { return index > 0 && index < 40 ? gObj[index] : nullptr; }

}  // namespace

// ---- the base window's virtuals and constructor (the real ones are in files this library does not compile)
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

// ---- constructors under their mangled names
extern "C" void FakeCoolRectCtor(void* self) __asm__("__ZN8CoolRectC1Ev");
extern "C" void FakeCoolRectCtor(void* self) {
    if (on()) ev("CoolRect", {newlab(self)});
}
extern "C" void FakeCoolRectDtor(void* self) __asm__("__ZN8CoolRectD1Ev");
extern "C" void FakeCoolRectDtor(void* self) {
    if (on()) ev("~CoolRect", {lab(self)});
}
extern "C" void FakeSpriteCtor(void* self, void* data) __asm__("__ZN34InterfaceCursorPosTextObjectSpriteC1EPv");
extern "C" void FakeSpriteCtor(void* self, void* data) {
    ev("InterfaceCursorPosTextObjectSprite", {newlab(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}
extern "C" void FakeSystemCtor(CFSOnlineSystem* self) __asm__("__ZN15CFSOnlineSystemC1Ev");
extern "C" void FakeSystemCtor(CFSOnlineSystem* self) {
    ev("CFSOnlineSystem", {newlab(self)});
    self->mParksByPlayer = reinterpret_cast<CFSOnlineSystemParkList_InWorldByPlayerName*>(gListBuf);
}

// ---- the fakes
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lf(callback)});
    return ObjOf(ret("Build"));
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0x7777) return nullptr;
    return ObjOf(1 + static_cast<uint32_t>(id) % 31);
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) { ev("SetCallbackFunction", {lab(this), lf(callback)}); }
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return ObjOf(ret("CreateObject"));
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), lab(rect), subtract});
    return 0;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
void InterfaceWindow::unlink() { ev("unlink", {lab(this)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), lf(makeRow), lf(attach)});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 0;
}
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 0;
}
int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {lab(this), lab(text), capacity});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {lab(this), lab(object)}); }
void InterfaceEditBox::SelectAll() { ev("SelectAll", {lab(this)}); }
int8_t InterfaceButton::GetState() {
    ev("GetState", {lab(this)});
    return static_cast<int8_t>(ret("GetState"));
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), static_cast<uint8_t>(state), static_cast<uint8_t>(redraw)});
    return 0;
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(gDrawDummy);
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(gFontDummy);
}
void MoveHelpText(int16_t x, int16_t y) { ev("MoveHelpText", {x, y}); }
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}
void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
void CFSOnlineSystemParkList_InWorldByPlayerName::GetList(const char16_t* playerName, Callback callback, const void* context) {
    ev("GetList", {lab(this), lab(playerName), lf(callback), lab(context)});
}
void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(), void (*onDone)(int32_t, const void*),
                               void* context) {
    ev("CreateOnlineParkInfoPanel",
       {static_cast<int32_t>(reinterpret_cast<uintptr_t>(park)), lab(parent), lf(onCancel), lf(onDone), lab(context)});
}
void ShowFindListSearching() { ev("ShowFindListSearching", {}); }
void RemoveOnlineParkFindPanel() { ev("RemoveOnlineParkFindPanel", {}); }
void CloseCb() { ev("CloseCb", {}); }

// ---- the callbacks, known only by address
int32_t OnlinePanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return DISTINCT(); }
int32_t OnlinePanelSubPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return DISTINCT(); }
int32_t ContactListHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return DISTINCT(); }
int32_t OnlineEditMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return DISTINCT(); }
int32_t ButtonsMenuMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return DISTINCT(); }
int32_t ButtonsUnderlayMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return DISTINCT(); }
InterfaceWindow* ContactsLCFormat() { return reinterpret_cast<InterfaceWindow*>(static_cast<intptr_t>(DISTINCT())); }
void ContactsLCPostFormat(InterfaceWindow*) { DISTINCT(); }
InterfaceWindow* MessagesLCFormat() { return reinterpret_cast<InterfaceWindow*>(static_cast<intptr_t>(DISTINCT())); }
void MessagesLCPostFormat(InterfaceWindow*) { DISTINCT(); }
void FindParksPostGetPlayerParksCallbackFn(int32_t, const void*, TbPtrList<CFSOnlinePark>*) { DISTINCT(); }
void GeneralParkDownloadCompleteCallback(int32_t, const void*) { DISTINCT(); }

extern "C" {

// The host address of a global, a function or a world block by the module's name for it (ADDR's keys, and obj0 to obj39).
uint64_t tpw_opfp_addr(const char* name) {
    MakeObjects();
    struct Entry {
        const char* name;
        const void* address;
    };
#define G(x) {#x, &x}
#define F(x) {#x, reinterpret_cast<const void*>(&x)}
    const Entry entries[] = {
        G(gOnlineSystem),
        G(gLobby),
        G(gUIRootWindow),
        G(gOnlinePanel),
        G(gOnlineWindow),
        G(gOnlineWindowE8c),
        G(gOnlineWindowE90),
        G(gOnlineContactsBusy),
        G(gOnlineCursorState),
        G(gOnlinePanelShown),
        G(gOnlineByte78),
        G(gOnlineWord88),
        G(gOnlineEditFontIndex),
        G(gGameWindowCentreX),
        G(gGameWindowCentreY),
        G(gGameWindowHalfWidth),
        G(gGameWindowHalfHeight),
        G(gScreenCentreX),
        G(gScreenCentreY),
        G(gViewCentreX),
        G(gViewCentreY),
        G(gFindPanelWord),
        G(gFindPanelCloseCallback),
        G(gFindPanelPlayerName),
        G(gOnlineEditText),
        G(gOnlineEmptyHelp),
        G(gOnlinePanelScript),
        G(gOnlineUnderlayScriptA),
        G(gOnlineUnderlayScriptB),
        F(OnlinePanelMsg),
        F(OnlinePanelSubPanelMsg),
        F(ContactListHandler),
        F(OnlineEditMsg),
        F(ButtonsMenuMsg),
        F(ButtonsUnderlayMsg),
        F(ContactsLCFormat),
        F(ContactsLCPostFormat),
        F(MessagesLCFormat),
        F(MessagesLCPostFormat),
        F(FindParksPostGetPlayerParksCallbackFn),
        F(GeneralParkDownloadCompleteCallback),
        {"draw", gDrawDummy},
        {"font", gFontDummy},
        {"list", gListBuf},
        {"sys", gSysBuf},
        {"lobby", gLobbyBuf},
        {"closecb", reinterpret_cast<const void*>(&CloseCb)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    int index = -1;
    if (std::sscanf(name, "obj%d", &index) == 1 && index >= 0 && index < 40) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gObj[index]));
    return 0;
}

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_opfp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host side of the Mac words the module writes by offset: the system's list at 0xec and the lobby's window at 4.
void tpw_opfp_link_sys() {
    reinterpret_cast<CFSOnlineSystem*>(gSysBuf)->mParksByPlayer = reinterpret_cast<CFSOnlineSystemParkList_InWorldByPlayerName*>(gListBuf);
}
void tpw_opfp_link_lobby(uint64_t window) { reinterpret_cast<CLBLobby*>(gLobbyBuf)->mWindow = reinterpret_cast<InterfaceWindow*>(window); }

int32_t tpw_opfp_layout(int64_t* out) {
    MakeObjects();
    int32_t n = 0;
    CFSOnlineSystem* sys = reinterpret_cast<CFSOnlineSystem*>(gSysBuf);
    CLBLobby* lobby = reinterpret_cast<CLBLobby*>(gLobbyBuf);
    out[n++] = reinterpret_cast<char*>(&sys->mParksByPlayer) - gSysBuf;
    out[n++] = reinterpret_cast<char*>(&lobby->mWindow) - gLobbyBuf;
    return n;
}

uint32_t tpw_opfp_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(OnlineParkFindPanelMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b));
}
void tpw_opfp_create() { CreateOnlinePanel(); }

}  // extern "C"
