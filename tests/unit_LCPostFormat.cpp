// Test library for the staff room panel's list formatter and message functions (unit v5004; libtpw_unit_LCPostFormat.dylib,
// tests/test_LCPostFormat.py), main app:
//   src/game/ui/ui_staffRoomStatus.cpp   LCPostFormat, StaffRoomPanelLCHandler, StaffRoomStatusMsg
//
// Every callee outside the unit is a fake that records what the real one reads (the same event names and arguments as the module's
// PowerPC stubs). The globals of other files the unit reads are defined here. The InterfaceWindow virtuals are defined here (a fake
// window needs a vtable for LCPostFormat's SetValue call); the two draw object constructors are fakes bound to the Itanium symbols.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
StaffRoomStatusPanel gStaffRoomStatusPanel;
StaffStatusPanel gStaffStatusPanel;
uint32_t gLCFontIndex;
int32_t gStaffRoomMsg406;
CThing* gStaffRoomPanelThing;
StaffType gStaffRoomStaffType;

// What the fakes answer (written by the test through tpw_lc_addr).
uint32_t gFakeData;      // GetData's out word
uint64_t gFakeWin;       // GetWindow's answer (a host pointer)

namespace {
inline uint32_t P32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
// A pointer made from a 32-bit arena offset (Ptr32), back to the offset; NULL is 0.
inline uint32_t Off(const void* p) {
    return p == nullptr ? 0u : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - (uintptr_t)(tpw::gArenaBase));
}
}  // namespace

// ---- the fake window's vtable: every slot is this function (the virtual call LCPostFormat makes is SetValue, slot 8 of the Itanium table)
static int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}
static void* gFakeVtable[24];

// ---- non-virtual window fakes
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), P32(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), P32(text)}); }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return mChild;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(gFakeWin));
}
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return static_cast<int32_t>(ret("startAnimTimer"));
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(gFakeData);
    return 1;
}
void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {lab(this), position, item[1].mInt, columns, update, unused});
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetFontTableEntry"))));
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("IndentClient", {lab(window), a, b}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), P32(thing)}); }
void CreateAllStaffPanel(StaffType type, CThing* thing) { ev("CreateAllStaffPanel", {static_cast<int32_t>(type), P32(thing)}); }
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultStatusPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultStatusPanelMsg"));
}
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisor", {static_cast<int32_t>(message)}); }
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
void StaffStatusPanel::Show(CThing* thing) { ev("Show", {lab(this), Off(thing)}); }

void StaffRoomStatusPanel::FillStaffRoomList(StaffType type, int32_t flag) {
    ev("FillStaffRoomList", {lab(this), static_cast<int32_t>(type), flag});
}
void StaffRoomStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void StaffRoomStatusPanel::UpdateViewSprite(CStaff* staff) { ev("UpdateViewSprite", {lab(this), Off(staff)}); }
void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
void UIStatusPanel::EraseObject() { ev("EraseObject", {lab(this)}); }
void UIStatusPanel::Move() { ev("Move", {lab(this)}); }

// ---- the two draw object constructors, bound to the symbols the complete-object constructors have
extern "C" void tpw_fake_sprite_ctor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void tpw_fake_sprite_ctor(void* self, void* data) {
    int64_t label = newlab(self);
    ev("SpriteCtor", {label, P32(data)});
}
extern "C" void tpw_fake_gradient_ctor(void* self, void* mesh) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
extern "C" void tpw_fake_gradient_ctor(void* self, void* mesh) {
    int64_t label = newlab(self);
    ev("GradientCtor", {label}, {static_cast<const char*>(mesh)});
}

extern "C" {

uint64_t tpw_lc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"font", &gLCFontIndex},         {"g406", &gStaffRoomMsg406},        {"thing", &gStaffRoomPanelThing},
        {"stafftype", &gStaffRoomStaffType}, {"panel", &gStaffRoomStatusPanel}, {"ssp", &gStaffStatusPanel},
        {"fakedata", &gFakeData},        {"fakewin", &gFakeWin},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lc_base() { return static_cast<uint64_t>((uintptr_t)(tpw::gArenaBase)); }

uint64_t tpw_lc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A window with a vtable (LCPostFormat calls a virtual on the child).
uint64_t tpw_lc_make_window() {
    void* block = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow) + 64);
    for (void*& slot : gFakeVtable) slot = reinterpret_cast<void*>(&FakeSetValue);
    *static_cast<void***>(block) = gFakeVtable;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int32_t tpw_lc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mScript));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mF28));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mF2c));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mChild));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceListControl2, mItemCount));
    return n;
}
#pragma clang diagnostic pop

void tpw_lc_LCPostFormat(uint64_t window) { LCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }
int32_t tpw_lc_Handler(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return StaffRoomPanelLCHandler(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b);
}
int32_t tpw_lc_Msg(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return StaffRoomStatusMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b);
}

}  // extern "C"
