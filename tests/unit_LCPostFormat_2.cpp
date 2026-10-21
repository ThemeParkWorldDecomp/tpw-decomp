// Test library for the online outbox panel (unit v5096; libtpw_unit_LCPostFormat_2.dylib, tests/test_LCPostFormat_2.py), main app:
//   src/game/ui/ui_onlineOutbox.cpp   OutboxLCFormat 0x101e4bdc, OutboxLCPostFormat 0x101e4d20, CreateOnlineOutboxPanel 0x101e4f28
//
// Every callee outside the unit is a fake that records what the real one reads (windows are plain numbers the fakes make up, never
// dereferenced; the objects the code makes are labelled by their constructor fakes). The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSUserInfo* gUserInfo;
InterfaceWindow* gOutboxWindow;
CLBScreenSystem* gLobbyScreens;
int16_t gUIRealScreenHeight;
int16_t gOutboxPanelData[2] = {0, 1};

// ---- the untranslated message functions: only their addresses matter
int32_t OutboxPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t AdressesTextPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {

alignas(16) uint8_t gFontStorage[0x40];
alignas(16) uint8_t gScreensStorage[sizeof(CLBScreenSystem) + 16];
CFSUserInfo* gPreUser = nullptr;

int64_t P(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }
int64_t Hash(const char* s) {
    int64_t sum = 0;
    int64_t len = 0;
    for (; *s; ++s, ++len) sum += static_cast<unsigned char>(*s);
    return len * 1000003 + sum;
}

}  // namespace

// ---- constructors the translation calls (the real ones need vtables this library does not have): the symbols themselves
// (top-level assembly: each constructor's complete and base symbol branches to the fake)
__asm__(
    ".text\n.p2align 2\n"
    ".globl __ZN15InterfaceWindowC1EPS_iissss\n.globl __ZN15InterfaceWindowC2EPS_iissss\n"
    "__ZN15InterfaceWindowC1EPS_iissss:\n__ZN15InterfaceWindowC2EPS_iissss:\n    b _FakeWindowCtor\n"
    ".globl __ZN25InterfaceTextObjectSpriteC1EPv\n.globl __ZN25InterfaceTextObjectSpriteC2EPv\n"
    "__ZN25InterfaceTextObjectSpriteC1EPv:\n__ZN25InterfaceTextObjectSpriteC2EPv:\n    b _FakeTextCtor\n"
    ".globl __ZN25InterfaceDrawObjectSpriteC1EPv\n.globl __ZN25InterfaceDrawObjectSpriteC2EPv\n"
    "__ZN25InterfaceDrawObjectSpriteC1EPv:\n__ZN25InterfaceDrawObjectSpriteC2EPv:\n    b _FakeSpriteCtor\n"
    ".globl __ZN11CFSUserInfoC1Ev\n.globl __ZN11CFSUserInfoC2Ev\n"
    "__ZN11CFSUserInfoC1Ev:\n__ZN11CFSUserInfoC2Ev:\n    b _FakeUserCtor\n");
extern "C" void FakeWindowCtor(InterfaceWindow* self, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                               int16_t right, int16_t bottom) {
    ev("WindowCtor", {newlab(self), P(parent), attributes, id, left, top, right, bottom});
}
extern "C" void FakeTextCtor(void* self, void* data) { ev("TextSpriteCtor", {newlab(self), P(data)}); }
extern "C" void FakeSpriteCtor(void* self, void* data) { ev("DrawSpriteCtor", {newlab(self), P(data)}); }
extern "C" void FakeUserCtor(CFSUserInfo* self) {
    ev("UserCtor", {newlab(self)});
    new (&self->mOutbox.mF28) std::list<uint32_t>(static_cast<size_t>(ret("ctorMail")));
}

// ---- the other callees
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFont", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFontStorage);
}
int32_t InterfaceFont::GetHeight() { return mHeight; }

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {P(this), lab(object)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {P(this), red, green, blue, alpha});
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {P(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {P(this), P(text)}); }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {P(this), unused});
    return reinterpret_cast<InterfaceWindow*>(P(this) + 0x100);
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {P(this)});
    return reinterpret_cast<InterfaceWindow*>(P(this) + 0x10);
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {P(this), horizontal, vertical}); }
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) { ev("SetFontWrap", {P(this), horizontal, vertical}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {P(this), P(object)}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {P(this), id});
    return reinterpret_cast<InterfaceWindow*>(0x71000000 + id);
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {P(this), lab(reinterpret_cast<const void*>(callback))});
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {P(this), on}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {Hash(name)});
    return reinterpret_cast<InterfaceDrawObject*>(0x72000000 + std::strlen(name));
}

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {P(this), lab(reinterpret_cast<const void*>(makeRow)), lab(reinterpret_cast<const void*>(attach))});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {P(this), column, static_cast<int64_t>(type)}); }
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {P(this), P(object)}); }

InterfaceWindow* Dialog(int16_t* script, int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t), int32_t modal) {
    ev("Dialog", {lab(script), lab(reinterpret_cast<const void*>(handler)), modal});
    return reinterpret_cast<InterfaceWindow*>(0x70000000);
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(0x74000000 + id);
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("UIMain_SetTitleText", {P(window), id, P(text)}); }
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {P(window), P(text), a, b, c, d, e});
}
void FillOutboxList() { ev("FillOutboxList", {}); }
void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    ev("AdvisorAdd", {P(this), response.mId, clearFirst});
}
TbResult TbRenderArea::AllocMemory() {
    ev("AllocMemory", {});
    return TbResult{};
}

extern "C" {

uint64_t tpw_lc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"font", gFontStorage},
        {"OutboxPanelMsg", reinterpret_cast<const void*>(&OutboxPanelMsg)},
        {"AdressesTextPanelMsg", reinterpret_cast<const void*>(&AdressesTextPanelMsg)},
        {"OutboxLCFormat", reinterpret_cast<const void*>(&OutboxLCFormat)},
        {"OutboxLCPostFormat", reinterpret_cast<const void*>(&OutboxLCPostFormat)},
        {"data", gOutboxPanelData},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lc_layout(int64_t* out) {
    (void)out;
    return 0;
}

// The world: the user info (0 none, 1 one with `mail` entries), the window global, the font's height and the screen's.
void tpw_lc_world(int32_t userKind, int32_t mail, uint32_t outboxWindow, int32_t fontHeight, int32_t screenHeight) {
    gPreUser = nullptr;
    gUserInfo = nullptr;
    if (userKind != 0) {
        gPreUser = static_cast<CFSUserInfo*>(tpw::ArenaAllocZeroed(sizeof(CFSUserInfo)));
        new (&gPreUser->mOutbox.mF28) std::list<uint32_t>(static_cast<size_t>(mail));
        gUserInfo = gPreUser;
    }
    gOutboxWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(outboxWindow));
    reinterpret_cast<InterfaceFont*>(gFontStorage)->mHeight = static_cast<int16_t>(fontHeight);
    gUIRealScreenHeight = static_cast<int16_t>(screenHeight);
    CLBScreenSystem* screens = reinterpret_cast<CLBScreenSystem*>(gScreensStorage);
    screens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(static_cast<uintptr_t>(0x75000000));
    gLobbyScreens = screens;
}

// out: the window global, then what gUserInfo is (0 none, 1 the one the world made, 2 another)
int32_t tpw_lc_state(int64_t* out) {
    out[0] = P(gOutboxWindow);
    out[1] = gUserInfo == nullptr ? 0 : (gUserInfo == gPreUser ? 1 : 2);
    return 2;
}

uint64_t tpw_lc_format() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(OutboxLCFormat())); }
void tpw_lc_post(uint32_t window) { OutboxLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }
int32_t tpw_lc_create() { return CreateOnlineOutboxPanel(); }

}  // extern "C"
