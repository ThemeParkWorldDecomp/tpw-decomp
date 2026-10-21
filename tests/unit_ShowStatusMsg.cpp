// Test library for ShowStatusMsg (unit v5525; libtpw_unit_ShowStatusMsg.dylib, tests/test_ShowStatusMsg.py), main app:
//   src/game/ui/ui_showStatus.cpp   ShowStatusMsg
//
// Every callee is a recording fake with the PowerPC stub's event name and arguments. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "analyser.h"
#include "ui_iterators.h"
#include "ui_main.h"

using namespace calllog;

// The panel is bigger than the empty class: a padded block under the same symbol.
struct PanelBlock {
    ShowStatusPanel panel;
    uint8_t pad[0x80];
};
PanelBlock gPanelBlock __asm__("_gShowStatusPanel");
CThing* gStaffRoomPanelThing;

int32_t _applytoall(CThing*) { return 0; }

uint64_t SlotFake(void* self) {
    ev("Slot", {lab(self)});
    return 0;
}

void UIMain_TickRidePreview() { ev("TickRidePreview", {}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int32_t>(message)}); }
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}
void ShowStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void ShowStatusPanel::UpdateStatusWindow() { ev("UpdateStatusWindow", {lab(this)}); }
void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
void UIStatusPanel::EraseObject() { ev("EraseObject", {lab(this)}); }
void UIStatusPanel::Move() { ev("Move", {lab(this)}); }
void UIStatusPanel::CloseObject(int32_t flag) { ev("CloseObject", {lab(this), flag}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), lab(thing)}); }
int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("IterateThingList", {static_cast<int32_t>(mask), fn == _applytoall ? 1 : 0});
    return 0;
}
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultStatusPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultStatusPanelMsg"));
}

extern "C" {

uint64_t tpw_ss_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gPanelBlock},
        {"slotfn", reinterpret_cast<const void*>(&SlotFake)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ss_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_ss_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

void tpw_ss_setthing(uint64_t address) { gStaffRoomPanelThing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(address)); }

int32_t tpw_ss_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(UIStatusPanelFields));
    return n;
}

int32_t tpw_ss_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return ShowStatusMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
