// Test library for CreateOnlineLoginPanel (unit v5339; libtpw_unit_CreateOnlineLoginPanel.dylib, tests/test_CreateOnlineLoginPanel.py), main app:
//   src/game/ui/ui_onlineLogin.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read. The windows are six fixed blocks in the arena (the
// dialog and its five children); their virtual SetText is a fake. The constructors of the draw objects are fakes under their real symbols.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "FS_OnlineLoginPanel.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gOnlineLoginPanelFlag;
char16_t gOnlineLoginPassword[33];
char16_t gOnlineLoginName[33];

namespace {

void* gWin[6];
void* gVtable[40];

int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    int64_t known = lab(text);
    ev("SetText", {lab(self), known, known == -1 ? static_cast<int64_t>(reinterpret_cast<uintptr_t>(text) & 0xffffffffu) : 0});
    return 1;
}

int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }

}  // namespace

// ---- the fakes
InterfaceWindow* Dialog(int16_t* script, int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t), int32_t modal) {
    ev("Dialog", {lab(script), lab(reinterpret_cast<const void*>(handler)), modal});
    return static_cast<InterfaceWindow*>(gWin[0]);
}

int32_t LobbyOnlineLoginPanelDlgMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(ret("GetFontTableEntry")));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("UIMain_GetString")));
}

void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {lab(window)}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace lender_callee

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id >= 0x49 && id <= 0x4d) return static_cast<InterfaceWindow*>(gWin[id - 0x49 + 1]);
    return nullptr;
}

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), Raw(text)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), Raw(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {lab(this), lab(text), capacity});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {lab(this), Raw(object)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(ret("FindDrawObject")));
}

// The constructors, under their real symbols (no vtable needed).
extern "C" {
void FakeCursorCtor(void* self, void* data) __asm__("__ZN34InterfaceCursorPosTextObjectSpriteC1EPv");
void FakeSpriteCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void FakeTitleCtor(void* self, void* data) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
void FakePasswordCtor(void* self, void* data) __asm__("__ZN27InterfacePasswordTextObjectC1EPv");
void FakeCursorCtor(void* self, void* data) { ev("CursorCtor", {newlab(self), Raw(data)}); }
void FakeSpriteCtor(void* self, void* data) { ev("SpriteCtor", {newlab(self), Raw(data)}); }
void FakeTitleCtor(void* self, void* data) { ev("TitleCtor", {newlab(self), Raw(data)}); }
void FakePasswordCtor(void* self, void* data) { ev("PasswordCtor", {newlab(self), Raw(data)}); }

// The host address of a global by the module's name for it (ADDR's keys); the windows by their own address.
uint64_t tpw_cr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"script", gOnlineLoginPanelScript}, {"empty", gOnlineLoginEmptyText},     {"callback", &gOnlineLoginFailureFn},
        {"flag", &gOnlineLoginPanelFlag},    {"pass", gOnlineLoginPassword},       {"name", gOnlineLoginName},
        {"handler", reinterpret_cast<const void*>(&LobbyOnlineLoginPanelDlgMsg)},
        {"win0", gWin[0]},                   {"win1", gWin[1]},                    {"win2", gWin[2]},
        {"win3", gWin[3]},                   {"win4", gWin[4]},                    {"win5", gWin[5]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// Makes the six window blocks once (32-bit addressable) with the fake virtual table.
void tpw_cr_init() {
    if (gWin[0] != nullptr) return;
    for (void*& entry : gVtable) entry = reinterpret_cast<void*>(&FakeSetText);
    for (void*& window : gWin) {
        window = tpw::ArenaAllocZeroed(0x400);
        *static_cast<void***>(window) = gVtable;
    }
}

int32_t tpw_cr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gOnlineLoginPanelFlag));
    out[n++] = static_cast<int64_t>(sizeof(gOnlineLoginName) / sizeof(char16_t));
    return n;
}

uint32_t tpw_cr_create(uint32_t onFailure) {
    CreateOnlineLoginPanel(reinterpret_cast<void (*)()>(static_cast<uintptr_t>(onFailure)));
    return 0;
}

}  // extern "C"
