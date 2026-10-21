// Test library for the staff and kid locator panels (unit v5343; libtpw_unit_ShowKidLocatorPanel.dylib, tests/test_ShowKidLocatorPanel.py), main app:
//   src/game/ui/ui_staffPanel.cpp   ShowStaffPanel, ShowKidLocatorPanel
//
// The callees outside the unit are fakes that record the arguments the real ones read; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
uint32_t gFirstPersonFlags;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
InterfaceWindow* gMainPanel;
int32_t gStaffPanelLocation;
int32_t gStaffPanelState;
CThing* gStaffPanelThing;
InterfaceWindow* gStaffPanelA;
InterfaceWindow* gStaffPanelB;
InterfaceWindow* gStaffPanelC;

namespace {

class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};
FakeMode gFakeMode;

}  // namespace

// ---- the fakes
void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n) { ev("LinkSlideoutPanel", {lab(panel), n}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return this;
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

void InterfaceWindow::link(InterfaceWindow* child, char a, char b) { ev("link", {lab(this), lab(child), a & 0xff, b & 0xff}); }

int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 0;
}

void UpdateLocator() { ev("UpdateLocator", {}); }
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int64_t>(message)}); }

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

extern "C" {

uint64_t tpw_sp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFirstPersonFlags}, {"w1e0", &gMouseWord1E0},       {"w1e4", &gMouseWord1E4},
        {"mode", &gMouseMode},         {"mouse", &gMouse},             {"main", &gMainPanel},
        {"loc", &gStaffPanelLocation}, {"state", &gStaffPanelState},   {"thing", &gStaffPanelThing},
        {"C", &gStaffPanelC},          {"B", &gStaffPanelB},           {"A", &gStaffPanelA},
        {"fakemode", &gFakeMode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    return n;
}

void tpw_sp_staff(uint64_t thing) { ShowStaffPanel(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }
void tpw_sp_kid() { ShowKidLocatorPanel(); }

}  // extern "C"
