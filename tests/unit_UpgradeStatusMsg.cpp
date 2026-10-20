// Test library for the upgrade status panel's window handler and the staff status static initialiser (unit v5548;
// libtpw_unit_UpgradeStatusMsg.dylib, tests/test_UpgradeStatusMsg.py), main app:
//   src/UpgradeStatusMsg.cpp   UpgradeStatusMsg, __sinit_ui_staffStatus_cpp, gStaffStatusF40, gStaffStatusF44
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. Objects are named by
// labels the test registers (tpw_us_setlab); the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here. The panels are bigger than their empty classes: the storage takes their names.
uint8_t gStaffPanelStorage[0x40] __asm__("_gStaffStatusPanel");
uint8_t gUpgradePanelStorage[0x40] __asm__("_gUpgradeStatusPanel");
CThing* gStaffRoomPanelThing;
CWorld* gGameWorld;

namespace {

uint64_t gLabels[16];

int64_t Lb(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 1; i < 16; i++)
        if (gLabels[i] == reinterpret_cast<uintptr_t>(p)) return i;
    return -1;
}

}  // namespace

// ---- the fakes
void UIMain_TickRidePreview() { ev("Tick", {}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("Cancel", {static_cast<int64_t>(message)}); }
void UpgradeStatusPanel::Remove() { ev("Remove", {Lb(this)}); }
void UIStatusPanel::GetNextObject() { ev("Next", {Lb(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("Prev", {Lb(this)}); }
void UIStatusPanel::EraseObject() { ev("Erase", {Lb(this)}); }
void UIStatusPanel::Move() { ev("Move", {Lb(this)}); }
void UIStatusPanel_Construct(UIStatusPanel* self) { ev("UICtor", {Lb(self)}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAll", {static_cast<int64_t>(type), Lb(thing)}); }
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {Lb(window), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("SetAnim", {Lb(this), id, ms});
    return static_cast<int16_t>(ret("SetAnim"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("StartAnim", {Lb(this), which});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {Lb(this), id});
    if (id == 0x5210) return reinterpret_cast<InterfaceWindow*>(gLabels[2]);
    if (id == 0x520e) return reinterpret_cast<InterfaceWindow*>(gLabels[3]);
    return nullptr;
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {Lb(this)});
    return nullptr;
}
uint32_t CObject::GetCurrentResidualValue() const {
    ev("Residual", {Lb(this)});
    return static_cast<uint32_t>(ret("Residual"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {Lb(this)});
    return reinterpret_cast<CParkAnalyser*>(gLabels[5]);
}
int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    ev("NumberOfObject", {Lb(this), id.mIndex});
    return static_cast<int32_t>(ret("NumberOfObject"));
}
namespace lender_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {Lb(window), value}); }
}  // namespace lender_callee

extern "C" {

uint64_t tpw_us_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"up", gUpgradePanelStorage},     {"staff", gStaffPanelStorage}, {"thing", &gStaffRoomPanelThing},
        {"world", &gGameWorld},           {"f40", &gStaffStatusF40},     {"f44", &gStaffStatusF44},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_us_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word of a host block.
uint32_t tpw_us_ptr(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(address)); }

void tpw_us_setlab(int32_t index, uint64_t address) { gLabels[index & 15] = address; }

int32_t tpw_us_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mMode));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF10));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}

int32_t tpw_us_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return UpgradeStatusMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

void tpw_us_sinit() { __sinit_ui_staffStatus_cpp(); }

}  // extern "C"
