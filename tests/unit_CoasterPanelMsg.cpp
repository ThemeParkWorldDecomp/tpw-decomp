// Test library for the coaster edit panel's message handler (unit v5540; libtpw_unit_CoasterPanelMsg.dylib, tests/test_CoasterPanelMsg.py), main app:
//   src/game/ui/ui_coasterPanel.cpp   CoasterPanelMsg
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the function reads or writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "TPCoaster.h"
#include "keyboard.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
CKeyTable* gKeyTableCoaster;
CKeyTable* gKeyTableCamera;
uint8_t gCoasterEditBusy;
void* gCoasterEditObject;
int32_t gCoasterEditMode;
InterfaceWindow* gCoasterPanel;

namespace {

// The mouse mode gMouseMode points to: only GetMode is ever asked (the PowerPC side asks it through the glue stub).
class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};

FakeMode gFakeMode;

}  // namespace

// ---- the fakes (the same event name and arguments as the module's PowerPC stubs)
void HandleCoasterButtonClick(uint32_t buttons, uint32_t event) { ev("HandleCoasterButtonClick", {buttons, event}); }

int32_t LLCC_SystemGetMode() {
    ev("LLCC_SystemGetMode", {});
    return static_cast<int32_t>(ret("LLCC_SystemGetMode"));
}

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag ? 1 : 0}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("LLCC_SystemSetRideTrack", {track}); }
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag ? 1 : 0}); }
void SetCoasterEdit(void* coaster, int32_t flag) { ev("SetCoasterEdit", {lab(coaster), flag}); }
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("UIMain_CancelAdvisorMessage", {static_cast<int32_t>(message)}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(message)}); }
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }

void GetCoasterEditAvailableButtons(uint32_t* out) {
    ev("GetButtons", {});
    *out = static_cast<uint32_t>(ret("GetButtons"));
}

int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    ev("KeyDown", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("KeyDown"));
}

int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    ev("KeyUp", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("KeyUp"));
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

// The mouse modes: constructors record, the rest is empty (the vtables need them).
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

CMMPathDraw::CMMPathDraw(CELL_MODES mode) {
    ev("CMMPathDraw", {newlab(this), static_cast<int32_t>(mode)});
}
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

uint64_t tpw_cpm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMouseWord1E0", &gMouseWord1E0}, {"gMouseWord1E4", &gMouseWord1E4}, {"gMouseMode", &gMouseMode},
        {"gMouse", &gMouse},               {"gKeyTableCoaster", &gKeyTableCoaster}, {"gKeyTableCamera", &gKeyTableCamera},
        {"gCoasterEditBusy", &gCoasterEditBusy}, {"gCoasterEditObject", &gCoasterEditObject},
        {"gCoasterEditMode", &gCoasterEditMode}, {"gCoasterPanel", &gCoasterPanel}, {"fakemode", &gFakeMode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cpm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cpm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CMouse, mF00));
    return n;
}

int32_t tpw_cpm_call(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return CoasterPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
