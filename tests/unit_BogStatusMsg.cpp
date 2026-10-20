// Test library for the bog status panel's message function, its static constructor, CoasterPlaceMouseMode and
// WhichCheckButtonsAreDown (unit v5699; libtpw_unit_BogStatusMsg.dylib, tests/test_BogStatusMsg.py), main app:
//   src/BogStatusPanel_InitViewWindow_BogStatusMsg.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CThing* gStaffRoomPanelThing;
InterfaceWindow* gCoasterPanel;
// gBogStatusPanel has no members in the header; the code writes the Mac layout (0x24 bytes) into it, so the storage is bigger.
alignas(16) char gBogStatusStorage[0x80] asm("_gBogStatusPanel");

static void* gButtonsPtr;

// ---- the mouse modes the unit constructs: constructors record, the virtuals only give the classes their vtables
CMMPathDraw::CMMPathDraw(CELL_MODES mode) { ev("CMMPathDraw", {newlab(this), static_cast<int64_t>(mode)}); }
CMMPathDraw::~CMMPathDraw() {}
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 3; }
void CMMPathDraw::OnAssign() {}

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

// ---- the other fakes
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag ? 1 : 0}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("LLCC_SystemSetRideTrack", {track}); }

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size < 64 ? 64 : size);
}
}  // namespace lender_callee

void UIStatusPanel_Construct(UIStatusPanel* self) { ev("UIStatusPanel", {lab(self)}); }

void UIMain_TickRidePreview() { ev("TickRidePreview", {}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int64_t>(message)}); }
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}
void BogStatusPanel::UpdateStatusWindow() { ev("UpdateStatusWindow", {lab(this)}); }
void BogStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
void UIStatusPanel::CloseObject(int32_t flag) { ev("CloseObject", {lab(this), flag}); }
void UIStatusPanel::RequestMaintenance(bool on) { ev("RequestMaintenance", {lab(this), on ? 1 : 0}); }
void UIStatusPanel::EraseObject() { ev("EraseObject", {lab(this)}); }
void UIStatusPanel::Move() { ev("Move", {lab(this)}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int64_t>(type), lab(thing)}); }
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultStatusPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultStatusPanelMsg"));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") != 0 ? static_cast<InterfaceWindow*>(gButtonsPtr) : nullptr;
}
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 0;
}
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }

extern "C" {

uint64_t tpw_bog_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", gBogStatusStorage},        {"thing", &gStaffRoomPanelThing}, {"coasterpanel", &gCoasterPanel},
        {"mouse", &gMouse},                  {"w1e0", &gMouseWord1E0},         {"w1e4", &gMouseWord1E4},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bog_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_bog_set_buttons(uint64_t address) { gButtonsPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

int32_t tpw_bog_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mMode));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}

int32_t tpw_bog_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return BogStatusMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
void tpw_bog_sinit() { __sinit_ui_bogStatus_cpp(); }
void tpw_bog_place() { CoasterPlaceMouseMode(); }
void tpw_bog_which(uint32_t mask) { WhichCheckButtonsAreDown(mask); }

}  // extern "C"
