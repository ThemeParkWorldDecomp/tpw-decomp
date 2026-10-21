// Test library for the online panel's message functions (unit v5658; libtpw_unit_OnlineEditMsg.dylib, tests/test_OnlineEditMsg.py), main app:
//   src/game/ui/ui_onlinePanel.cpp   OnlineEditMsg, OnlinePanelSubPanelMsg
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The globals of
// other files are defined here, zero at start like the Mac's bss. The three virtual calls and the allocator are the weak helpers of
// the translation, faked here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "online.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
char16_t gOnlineEmptyHelp[2];
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
CKeyTable* gKeyTableShortcuts;
uint8_t gOnlineBlockToggle;
uint8_t gOnlinePanelShown;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gOnlinePanel;
char16_t gOnlineEditText[256];
COnlineControl* gOnlineControl;

namespace {

int64_t Hash(const char16_t* text) {
    int64_t h = 0;
    for (; *text; ++text) h = (h * 31 + *text) & 0x7fffffff;
    return h;
}

void* Block(const char* name);

}  // namespace

// ---- the weak helpers of the translation
namespace online_edit_callee {
void* New(size_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(static_cast<uint32_t>(size));
}
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("SetText", {lab(window), Hash(text)}); }
int32_t MouseModeOf(CMouseMode* mode) {
    ev("GetMode", {lab(mode)});
    return static_cast<int32_t>(ret("GetMode"));
}
}  // namespace online_edit_callee

// ---- the fakes
void UIMain_DisableAllKeyTables() { ev("Disable", {}); }
void UIMain_EnableAllKeyTables() { ev("Enable", {}); }
void InterfaceWindow::UnCaptureKb(int32_t on) { ev("UnCaptureKb", {lab(this), on}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    const char* name = id == 0xc3123 ? "G1" : id == 0xc312b ? "G2" : "G3";
    return static_cast<InterfaceWindow*>(Block(name));
}
void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("Search"));
}
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}
CMMOnline::CMMOnline() { ev("CMMOnline", {newlab(this)}); }
CMMOnline::~CMMOnline() {}
void CMMOnline::OnLeftClick(int32_t, int32_t) {}
void CMMOnline::OnRightClick(int32_t, int32_t) {}
void CMMOnline::OnDoubleClick(int32_t, int32_t) {}
int32_t CMMOnline::GetMode() { return 7; }
void CMMOnline::OnAssign() {}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
COnlineControl::COnlineControl() { ev("COnlineControl", {newlab(this)}); }
void COnlineControl::SendText(std::u16string text) { ev("SendText", {lab(this), Hash(text.c_str())}); }
void CreateMainMenu(int32_t a) { ev("CreateMainMenu", {a}); }
void ContactMenu(int32_t a) { ev("ContactMenu", {a}); }

namespace {

// Persistent blocks standing for windows and objects; the Python side fills them.
void* Block(const char* name) {
    static const char* names[] = {"P", "A", "B", "G1", "G2", "G3", "KT", "MO", "MS", "CT", "VT1", "VT2"};
    static void* blocks[12];
    for (int i = 0; i < 12; i++)
        if (std::strcmp(names[i], name) == 0) {
            if (blocks[i] == nullptr) blocks[i] = tpw::ArenaAllocZeroed(0x140);
            return blocks[i];
        }
    return nullptr;
}

}  // namespace

extern "C" {

uint64_t tpw_online_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineEmptyHelp", &gOnlineEmptyHelp}, {"gMouseWord1E0", &gMouseWord1E0}, {"gMouseWord1E4", &gMouseWord1E4},
        {"gMouseMode", &gMouseMode},             {"gMouse", &gMouse},               {"gKeyTableShortcuts", &gKeyTableShortcuts},
        {"gOnlineBlockToggle", &gOnlineBlockToggle}, {"gOnlinePanelShown", &gOnlinePanelShown}, {"gOnlineWindow", &gOnlineWindow},
        {"gOnlinePanel", &gOnlinePanel},         {"gOnlineEditText", &gOnlineEditText}, {"gOnlineControl", &gOnlineControl},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Block(name)));
}

uint64_t tpw_online_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_online_layout(int64_t* out) {
    (void)out;
    return 0;
}

int32_t tpw_online_edit(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OnlineEditMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

int32_t tpw_online_sub(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OnlinePanelSubPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
