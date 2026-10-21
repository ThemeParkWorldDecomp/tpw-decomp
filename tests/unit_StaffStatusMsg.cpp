// Test library for the staff status panel's message functions (unit v5364; libtpw_unit_StaffStatusMsg.dylib, tests/test_StaffStatusMsg.py), main app:
//   src/game/ui/ui_staffStatus.cpp   StaffStatusMsg, OneEditMsg
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue. The globals of
// other files the unit reads are defined here. The panel object is raw bytes (the class has no members in the header).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouse* gMouse;
CThing* gStaffRoomPanelThing;
char16_t gStaffEditName[64];
// The panel is raw bytes under the symbol of the C++ global (the class is empty in the header).
alignas(16) uint8_t gPanelBytes[64] __asm__("_gStaffStatusPanel");

namespace {

bool gInNew = false;
void* gWinRet = nullptr;
void* gExtra[16];
void* gFakeVt[64];

int64_t I(int64_t v) { return v; }
int64_t Raw(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

int16_t FakeEnable(void* self, int16_t on) {
    ev("Enable", {I(self == gWinRet ? 1 : 0), I(on)});
    return 1;
}
void FakeSlot34(void* panel) { ev("Slot34", {lab(panel)}); }
void FakeSlot0c(void* obj) { ev("Slot0c", {I(obj == gExtra[1] ? 1 : 0)}); }

}  // namespace

// ---- operator new: records the size, never returns NULL, allocation of at least 64 bytes (the fake constructors write a vtable)
// (the shim library provides operator new and does not record it: the test checks the sizes the original asks for on the PowerPC side)

// ---- the fakes
void StaffStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void StaffStatusPanel::SetSelected() { ev("SetSelected", {lab(this)}); }
void StaffStatusPanel::UpdateStatusWindow() { ev("UpdateStatusWindow", {lab(this)}); }
void StaffStatusPanel::FireMe() { ev("FireMe", {lab(this)}); }
void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
void UIStatusPanel::EditNamePanelGone() { ev("EditNamePanelGone", {I(this == gExtra[1] ? 1 : 0)}); }

void UIMain_CancelAdvisorMessage(AdvisorMessage m) { ev("CancelAdvisorMessage", {I(static_cast<int32_t>(m))}); }
void UIMain_AdvisorMessage(AdvisorMessage m) { ev("AdvisorMessage", {I(static_cast<int32_t>(m))}); }
void UIMain_UpdateSpritePreview() { ev("UpdateSpritePreview", {}); }
void UIMain_EnableAllKeyTables() { ev("EnableAllKeyTables", {}); }
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {}); }

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {Raw(this), I(id), I(ms)});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {Raw(this), I(which)});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {I(tpw::ArenaOffset(this)), I(id)});   // the panel's window is a Ptr32 (an arena offset)
    return static_cast<InterfaceWindow*>(gWinRet);
}
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {Raw(this), I(index)});
    return (index >= 0 && index < 16) ? gExtra[index] : nullptr;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {Raw(this), I(message), I(a), I(b)});
    return static_cast<int32_t>(ret("CallDefault"));
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {Raw(window), I(message), I(a), I(b)});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {Raw(window), I(message), I(a), I(b)});
    return 0;
}
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultStatusPanelMsg", {Raw(window), I(message), I(a), I(b)});
    return static_cast<int32_t>(ret("Default"));
}

int32_t CStaff::CanBePickedUp() {
    ev("CanBePickedUp", {Raw(this)});
    return static_cast<int32_t>(ret("CanBePickedUp"));
}
StaffType CStaff::GetStaffType() {
    ev("GetStaffType", {Raw(this)});
    return static_cast<StaffType>(ret("GetStaffType"));
}
void CStaff::PickMeUp() { ev("PickMeUp", {Raw(this)}); }
void CStaff::CancelPatrolArea() { ev("CancelPatrolArea", {Raw(this)}); }
void CStaff::SetName(const char16_t* name) { ev("SetName", {Raw(this), lab(name)}); }
void CreateAllStaffPanel(StaffType type, CThing* thing) { ev("CreateAllStaffPanel", {I(static_cast<int32_t>(type)), Raw(thing)}); }
void ShowStaffPanel(CThing* thing) { ev("ShowStaffPanel", {Raw(thing)}); }

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
CMMPathDraw::CMMPathDraw(CELL_MODES mode) { ev("CMMPathDraw", {newlab(this), I(static_cast<int32_t>(mode))}); }
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }

// The vtables of the two modes (their key functions are not in this library): empty.
CMMPathDraw::~CMMPathDraw() {}
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 3; }
void CMMPathDraw::OnAssign() {}
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

extern "C" {

uint64_t tpw_sm_addr(const char* name) {
    for (void*& slot : gFakeVt) slot = reinterpret_cast<void*>(&FakeEnable);
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", gPanelBytes},
        {"thing", &gStaffRoomPanelThing},
        {"mouse", &gMouse},
        {"w1e0", &gMouseWord1E0},
        {"w1e4", &gMouseWord1E4},
        {"editname", gStaffEditName},
        {"vt", gFakeVt},
        {"winret", &gWinRet},
        {"extra", gExtra},
        {"fn_slot34", reinterpret_cast<const void*>(&FakeSlot34)},
        {"fn_slot0c", reinterpret_cast<const void*>(&FakeSlot0c)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of a host address (what a Ptr32 holds).
uint32_t tpw_sm_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_sm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}

uint32_t tpw_sm_StaffStatusMsg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(StaffStatusMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
uint32_t tpw_sm_OneEditMsg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(OneEditMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}

}  // extern "C"
