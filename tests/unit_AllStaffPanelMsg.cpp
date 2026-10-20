// Test library for the all-staff and finance panels' message functions and UIStatusPanel::RequestMaintenance (unit v51004;
// libtpw_unit_AllStaffPanelMsg.dylib, tests/test_AllStaffPanelMsg.py), main app:
//   src/MainWindowCallback_CreateAllItemsPanel_AllStaffPanelMsg.cpp
//
// Every callee outside the unit is a fake that records raw integers (pointers truncated to 32 bits) and answers from the test's queue.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gStaffSort;
InterfaceWindow* gStaffPanel;
InterfaceWindow* gFinanceGraphPanel;
CThing* gStaffRoomPanelThing;
StaffStatusPanel gStaffStatusPanel;

namespace {
int64_t P(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p)); }
int64_t R(const void* p) { return P(p); }
}  // namespace

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetString"))));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {P(window), id, P(text)}); }
void RemoveAllStaffPanel() { ev("RemoveAllStaffPanel", {}); }
void UpdateListEntries() { ev("UpdateListEntries", {}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {P(this), id});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetWindow"))));
}
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {P(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 0;
}
void StaffStatusPanel::Show(CThing* thing) { ev("Show", {this == &gStaffStatusPanel ? 1 : 0, P(thing)}); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {P(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("Zoom", {id.mIndex}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisor", {static_cast<int32_t>(message)}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), P(thing)}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }
void CreateAllKidsPanel(CThing* thing) { ev("CreateAllKidsPanel", {P(thing)}); }
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {P(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultPanelMsg"));
}
void RemoveFinancePanelPanel() { ev("RemoveFinance", {}); }
void CreateLoansPanel() { ev("CreateLoans", {}); }
void CreateStaffCostsPanel() { ev("CreateStaffCosts", {}); }
void CreateEntryPricePanel() { ev("CreateEntryPrice", {}); }
int32_t CObject::RequestMaintenance(int32_t on) {
    ev("ObjRequest", {static_cast<int64_t>(*reinterpret_cast<const uint32_t*>(this)), on});
    return 1;
}

namespace {
class FakeOps : public UIStatusPanelOps {
public:
    void Slot4() override { ev("Slot", {4}); }
    void Slot5() override { ev("Slot", {5}); }
    void Slot6() override { ev("Slot", {6}); }
};
}  // namespace

extern "C" {

uint64_t tpw_ash_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gStaffSort", &gStaffSort},
        {"gStaffPanel", &gStaffPanel},
        {"gFinanceGraphPanel", &gFinanceGraphPanel},
        {"gStaffRoomPanelThing", &gStaffRoomPanelThing},
        {"gStaffStatusPanel", &gStaffStatusPanel},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ash_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A fake table object in the arena; the answer is its arena offset (what a Ptr32 holds).
uint32_t tpw_ash_ops() {
    void* mem = tpw::ArenaAllocZeroed(sizeof(FakeOps));
    new (mem) FakeOps();
    return tpw::ArenaOffset(mem);
}

int32_t tpw_ash_layout(int64_t* out) {
    (void)out;
    return 0;
}

uint32_t tpw_ash_staff(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(AllStaffPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b));
}
uint32_t tpw_ash_finance(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(FinancePanelPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), m, a, b));
}
uint32_t tpw_ash_request(uint64_t panel, uint32_t on) {
    reinterpret_cast<UIStatusPanel*>(static_cast<uintptr_t>(panel))->RequestMaintenance(on != 0);
    return 0;
}

}  // extern "C"
