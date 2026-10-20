// Test library for RideStatusMsg (unit v5534; libtpw_unit_RideStatusMsg.dylib, tests/test_RideStatusMsg.py), main app:
//   src/sinit_ui_RideStatus_cpp_RideStatusMsg.cpp   RideStatusMsg
//
// Every callee is a fake recording the arguments the real one reads and answering from the test's queue; gRideStatusPanel and
// gStaffRoomPanelThing, defined in other files, are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ThingReceivers.h"

using namespace calllog;

RideStatusPanel gRideStatusPanel;
CThing* gStaffRoomPanelThing;

namespace {
struct FakeOps : UIStatusPanelOps {
    void Slot11(void* panel) override { ev("PtrGlue", {lab(panel)}); }
};
FakeOps gFakeOps;
}  // namespace

void UIMain_TickRidePreview() { ev("Tick", {}); }
void RideStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("Cancel", {static_cast<int32_t>(message)}); }
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("SetAnim", {lab(this), id, ms});
    return static_cast<int16_t>(ret("SetAnim"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("StartAnim", {lab(this), which});
    return 0;
}
void RideStatusPanel::UpdateStatusWindow() { ev("UpdateStatus", {lab(this)}); }
void RideStatusPanel::Edit(int32_t on) { ev("Edit", {lab(this), on}); }
void UIStatusPanel::Move() { ev("Move", {lab(this)}); }
void UIStatusPanel::CloseObject(int32_t flag) { ev("Close", {lab(this), flag}); }
void UIStatusPanel::RequestMaintenance(bool on) { ev("Maint", {lab(this), on ? 1 : 0}); }
void UIStatusPanel::EraseObject() { ev("Erase", {lab(this)}); }
void CObject::GoFirstPerson(int32_t on) const { ev("FirstPerson", {lab(this), on}); }
void RideStatusPanel::ShowUpgradePanel() { ev("ShowUpgrade", {lab(this)}); }
void UIStatusPanel::GetNextObject() { ev("Next", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("Prev", {lab(this)}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAll", {static_cast<int32_t>(type), lab(thing)}); }
void RideStatusPanel::DoDial(int32_t a, int32_t b) { ev("DoDial", {lab(this), a, b}); }
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

extern "C" {

uint64_t tpw_rsm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gRideStatusPanel},
        {"thing", &gStaffRoomPanelThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rsm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rsm_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_rsm_setops() { gRideStatusPanel.mOps = &gFakeOps; }

int32_t tpw_rsm_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return RideStatusMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
