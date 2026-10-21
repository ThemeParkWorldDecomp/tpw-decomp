// Test library for the coaster edit mode's end and its button enabling (unit v5538; libtpw_unit_EndCoasterEditMode.dylib,
// tests/test_EndCoasterEditMode.py), main app:
//   src/game/ui/ui_coasterPanel.cpp   EndCoasterEditMode, CoasterButtonsEnabled
//
// The callees outside the unit are fakes recording what the real ones read: RemoveSlideoutPanel, HandleCoasterButtonClick,
// LLCC_SystemGetMode/SetMode, SetCoasterEdit, UIMain_DecideKbCapture, CMMDefault's constructor, CMouse::AssignMouseMode,
// InterfaceWindow::GetWindow, the mouse mode's GetMode and the windows' Enable. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "TPCoaster.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gCoasterEditMode;
void* gCoasterEditObject;
InterfaceWindow* gCoasterPanel;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
CActionRecorder gActionRecorder;

namespace {

// A window as the code sees it: only the virtual table matters (Enable), so a block with the vptr of a class of the same shape.
struct FakeWin {
    virtual void DrawFunction() {}
    virtual void CalcClientArea() {}
    virtual ~FakeWin() {}
    virtual int32_t WhatAmI() { return 0; }
    virtual void ClientDraw() {}
    virtual int16_t Enable(int16_t on) {
        ev("Enable", {lab(this), static_cast<int64_t>(on & 0xff)});
        return 0;
    }
};

class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};

alignas(16) uint8_t gWinBlocks[0x40][0x140];
alignas(16) uint8_t gPanelBlock[0x140];
FakeMode gFakeMode;
bool gInit = false;

void InitWins() {
    if (gInit) return;
    gInit = true;
    FakeWin proto;
    void* vptr;
    std::memcpy(&vptr, static_cast<void*>(&proto), sizeof vptr);
    for (auto& block : gWinBlocks) std::memcpy(block, &vptr, sizeof vptr);
}

}  // namespace

// ---- the fakes
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void HandleCoasterButtonClick(uint32_t buttons, uint32_t event) { ev("HandleCoasterButtonClick", {buttons, event}); }
int32_t LLCC_SystemGetMode() {
    ev("LLCC_SystemGetMode", {});
    return static_cast<int32_t>(ret("LLCC_SystemGetMode"));
}
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag ? 1 : 0}); }
void SetCoasterEdit(void* coaster, int32_t flag) { ev("SetCoasterEdit", {lab(coaster), flag}); }
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    InitWins();
    ev("GetWindow", {lab(this), id});
    return (id >= 0 && id < 0x40) ? reinterpret_cast<InterfaceWindow*>(gWinBlocks[id]) : nullptr;
}

extern "C" {

uint64_t tpw_ec_addr(const char* name) {
    InitWins();
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"editmode", &gCoasterEditMode},   {"editobject", &gCoasterEditObject}, {"panelptr", &gCoasterPanel},
        {"w1e0", &gMouseWord1E0},          {"w1e4", &gMouseWord1E4},            {"modeptr", &gMouseMode},
        {"mouse", &gMouse},                {"recording", &gActionRecorder.mRecording},
        {"panel", gPanelBlock},            {"modeobj", static_cast<CMouseMode*>(&gFakeMode)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    if (std::strncmp(name, "win", 3) == 0) {
        int id = std::atoi(name + 3);
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWinBlocks[id & 0x3f]));
    }
    return 0;
}

uint64_t tpw_ec_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ec_layout(int64_t*) { return 0; }

void tpw_ec_end() { EndCoasterEditMode(); }
void tpw_ec_buttons(uint32_t buttons) { CoasterButtonsEnabled(buttons); }

}  // extern "C"
