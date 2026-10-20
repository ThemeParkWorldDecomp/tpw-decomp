// Test library for the online city info overlay and the online login panel's message function (unit v5100;
// libtpw_unit_UpdateOnlineCityInfoOverlay.dylib, tests/test_UpdateOnlineCityInfoOverlay.py), main app:
//   src/UI_IslandLobby_UpdateOnlineCityInfoOverlay.cpp   UpdateOnlineCityInfoOverlay, CreateOnlineCityInfoOverlay, OnlineLoginPanelDlgMsg
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The windows
// the fakes hand out are arena blocks with a fake vtable whose slot 10 is SetText (the virtual the overlay calls).
// The globals of other files the unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CLBLobby* gLobby = nullptr;
CFSOnlineSystem* gOnlineSystem = nullptr;
char16_t gUIMainConvertBuffer[0x4000];
int16_t gCityInfoOverlayScript[4] = {1, 0, 0, 0};
union TextHolder {
    TextHolder() {}
    ~TextHolder() {}
    CText t;
};
TextHolder gTextHolder __asm__("_gText");   // gText: the unit needs only its resource pointers; no constructor runs

// The Mac's constructor of the text sprite, defined by its symbol (the class has bases this library does not compile).
extern "C" void FakeSpriteCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeSpriteCtor(void* self, void* data) {
    ev("SpriteCtor", {lab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}

namespace {
// slots: 0 to 3 the windows of the ids 0x16c11, 0x16c12, 0x4b, 0x49; 4 the window Build answers; 5 the font; 6 to 9 the new blocks
uint64_t gSlot[10];
int gPoolNext = 0;
constexpr int32_t kIds[4] = {0x16c11, 0x16c12, 0x4b, 0x49};
void* gFakeVtable[16];

int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {lab(self), lab(text)});
    return 1;
}
}  // namespace

// ---- the fakes
void* online_callee::OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return reinterpret_cast<void*>(gSlot[6 + (gPoolNext++ & 3)]);
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gSlot[5]);
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), static_cast<int64_t>(id)});
    for (int i = 0; i < 4; ++i)
        if (kIds[i] == id) return reinterpret_cast<InterfaceWindow*>(gSlot[i]);
    return nullptr;
}

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<const void*>(callback))});
    return reinterpret_cast<InterfaceWindow*>(gSlot[4]);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}
void UIMain_DisableAllKeyTables() { ev("UIMain_DisableAllKeyTables", {}); }
void UIMain_EnableAllKeyTables() { ev("UIMain_EnableAllKeyTables", {}); }
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
void RemoveOnlineLoginPanel(int32_t arg) { ev("RemoveOnlineLoginPanel", {arg}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {lab(this)}); }
void CFSOnlineSystem::DoLogin(const char16_t* name, const char16_t* password) {
    ev("DoLogin", {lab(this), lab(name), lab(password)});
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    const TbIntegerVTag* tag = va_arg(args, const TbIntegerVTag*);
    va_end(args);
    ev("FormatString", {lab(this), lab(out), static_cast<int64_t>(size), static_cast<int64_t>(id),
                        static_cast<int64_t>(tag->mMagic), static_cast<int64_t>(tag->mId), static_cast<int64_t>(tag->mValue)});
    return 0;
}
// The key function of the tag's class lives in the text library; a body here gives the tag its vtable.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

int32_t CityInfoOverlayMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

extern "C" {

uint64_t tpw_ov_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"overlay", &gOnlineCityOverlay},
        {"shown", &gOnlineCityOverlayShown},
        {"flag", &gOnlineLoginPanelFlag},
        {"password", gOnlineLoginPassword},
        {"name", gOnlineLoginName},
        {"script", gCityInfoOverlayScript},
        {"cb", reinterpret_cast<const void*>(&CityInfoOverlayMsg)},
        {"notext", kOnlineCityNoText},
        {"buf", gUIMainConvertBuffer},
        {"sys", &gOnlineSystem},
        {"lobby", &gLobby},
        {"text6", &gTextHolder.t.mResourcePointers[6]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ov_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The fake vtable a window block points to (slot 10 is SetText).
uint64_t tpw_ov_vtable() {
    gFakeVtable[10] = reinterpret_cast<void*>(&FakeSetText);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gFakeVtable));
}

void tpw_ov_set(int32_t slot, uint64_t address) { gSlot[slot] = address; }

int32_t tpw_ov_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mName));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlineCity, mValue));
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlineCity));
    return n;
}

void tpw_ov_update(uint64_t city) {
    gPoolNext = 0;
    UpdateOnlineCityInfoOverlay(reinterpret_cast<const CFSOnlineCity*>(city));
}
void tpw_ov_create() {
    gPoolNext = 0;
    CreateOnlineCityInfoOverlay();
}
uint32_t tpw_ov_dlg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    gPoolNext = 0;
    return static_cast<uint32_t>(OnlineLoginPanelDlgMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b));
}

}  // extern "C"
