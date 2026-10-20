// Test library for the coaster edit mode (unit v5246; libtpw_unit_CoasterEditMode.dylib, tests/test_CoasterEditMode.py), main app:
//   src/ui_entryPricePanel_CoasterEditMode.cpp   CoasterEditMode, WhichSpecialButton; it defines gCoasterEditObject and gCoasterEditBusy
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The windows are
// blocks of arena memory whose first word is the vtable of a fake class with the same virtual layout (Enable is slot 6), the mouse mode
// is a fake CMouseMode. The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "CellBase.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
CActionRecorder gActionRecorder;
int32_t gCoasterEditMode;
InterfaceWindow* gCoasterPanel;

namespace {

constexpr int kWindows = 0x30;
constexpr size_t kWindowSize = 0x400;

// Same virtual layout as InterfaceWindow up to Enable: DrawFunction, CalcClientArea, ~ (two slots), WhatAmI, ClientDraw, Enable.
struct FakeWindowVt {
    virtual void F0() {}
    virtual void F1() {}
    virtual ~FakeWindowVt() {}
    virtual int32_t F4() { return 0; }
    virtual void F5() {}
    virtual int16_t Enable(int16_t on) {
        ev("Enable", {lab(this), on});
        return 0;
    }
};

struct FakeMode : CMouseMode {
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};

uint8_t* gWindows = nullptr;
FakeMode* gMode = nullptr;
void* gDrawObject = nullptr;

void* VtableOfFakeWindow() {
    static FakeWindowVt fake;
    return *reinterpret_cast<void**>(&fake);
}

uint8_t* Window(int id) {
    if (gWindows == nullptr) gWindows = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(kWindows * kWindowSize));
    uint8_t* w = gWindows + static_cast<size_t>(id) * kWindowSize;
    *reinterpret_cast<void**>(w) = VtableOfFakeWindow();
    return w;
}

}  // namespace

// ---- the fakes
void UI_SetSelectedObject(void* object) { ev("UI_SetSelectedObject", {lab(object)}); }
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag ? 1 : 0}); }
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n) { ev("LinkSlideoutPanel", {lab(panel), n}); }
int32_t IsTrackClosed(void* coaster) {
    ev("IsTrackClosed", {lab(coaster)});
    return static_cast<int32_t>(ret("IsTrackClosed"));
}
void SetCoasterEdit(void* coaster, int32_t flag) { ev("SetCoasterEdit", {lab(coaster), flag}); }
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("LLCC_SystemSetRideTrack", {track}); }
void GetCoasterEditAvailableButtons(uint32_t* out) {
    ev("GetCoasterEditAvailableButtons", {});
    *out = static_cast<uint32_t>(ret("GetCoasterEditAvailableButtons"));
}
void GetCoasterEditDefaultButtons(uint32_t* out) {
    ev("GetCoasterEditDefaultButtons", {});
    *out = static_cast<uint32_t>(ret("GetCoasterEditDefaultButtons"));
}
void WhichCheckButtonsAreDown(uint32_t mask) { ev("WhichCheckButtonsAreDown", {static_cast<int64_t>(mask)}); }

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(Window(id));
}
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return static_cast<InterfaceDrawObject*>(gDrawObject);
}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

// The mouse modes' constructors are fakes; their virtuals are empty (the vtables are needed by the constructors' stores).
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

CMMCoaster::CMMCoaster() { ev("CMMCoaster", {newlab(this)}); }
CMMCoaster::~CMMCoaster() {}
void CMMCoaster::OnLeftClick(int32_t, int32_t) {}
void CMMCoaster::OnLeftRelease(int32_t, int32_t) {}
void CMMCoaster::OnRightClick(int32_t, int32_t) {}
int32_t CMMCoaster::GetMode() { return 8; }
void CMMCoaster::OnAssign() {}

CMMPathDraw::CMMPathDraw(CELL_MODES mode) { ev("CMMPathDraw", {newlab(this), static_cast<int32_t>(mode)}); }
CMMPathDraw::~CMMPathDraw() {}
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 3; }
void CMMPathDraw::OnAssign() {}

extern "C" {

uint64_t tpw_cem_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"emode", &gCoasterEditMode},
        {"sel", &gCoasterEditObject},
        {"busy", &gCoasterEditBusy},
        {"panel", &gCoasterPanel},
        {"mouse", &gMouse},
        {"mmode", &gMouseMode},
        {"w1e0", &gMouseWord1E0},
        {"w1e4", &gMouseWord1E4},
        {"rec", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cem_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena may be recycled between cases: the fakes are made again at the start of each one.
void tpw_cem_reset() {
    gWindows = nullptr;
    gMode = nullptr;
    gDrawObject = nullptr;
}

// The fake window number `id` (its vtable set), the fake mouse mode and the fake draw object.
uint64_t tpw_cem_win(int32_t id) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Window(id))); }
uint64_t tpw_cem_mode() {
    if (gMode == nullptr) gMode = new (tpw::ArenaAllocZeroed(sizeof(FakeMode))) FakeMode();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMode));
}
uint64_t tpw_cem_drawobj() {
    if (gDrawObject == nullptr) gDrawObject = tpw::ArenaAllocZeroed(64);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gDrawObject));
}
int64_t tpw_cem_label(uint64_t p) { return lab(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

// The host offsets of the window fields the module reads back (the Mac's are 0x8c and 0x90).
int32_t tpw_cem_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<InterfaceWindow*>(0x1000)->mHelpText) - 0x1000);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<InterfaceWindow*>(0x1000)->mF90) - 0x1000);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    return n;
}

void tpw_cem_CoasterEditMode(uint64_t object) { CoasterEditMode(reinterpret_cast<void*>(static_cast<uintptr_t>(object))); }
void tpw_cem_WhichSpecialButton() { WhichSpecialButton(); }

}  // extern "C"
