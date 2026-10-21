// Test library for the ui_RideStatus.cpp static initialiser (unit v5300; libtpw_unit_sinit_ui_RideStatus_cpp.dylib):
//   src/game/ui/ui_RideStatus.cpp   __sinit_ui_RideStatus_cpp
//
// UIStatusPanel::UIStatusPanel is a recording fake. gRideStatusPanel and RideStatusPanelTable are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

void __sinit_ui_RideStatus_cpp();

RideStatusPanel gRideStatusPanel;

// The ride panel's table: the main app's is in src/game/ui/ui_StatusPanel.cpp, outside the unit (unit v5L31). This one is a block of the
// arena of its own, made on first use; only its address is compared.
uint32_t* RideStatusPanelTable() {
    static uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(32 * 4));
    return table;
}

void UIStatusPanel_Construct(UIStatusPanel* self) {
    ev("UIStatusPanel", {lab(self)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gRideStatusPanel},
        {"ops", RideStatusPanelTable()},
        {"f00", &gRideStatusPanel.mF00},
        {"f28", &gRideStatusPanel.mF28},
        {"mops", &gRideStatusPanel.mVtable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 value of a block (the panel's table is a Ptr32, unit v5L31).
uint32_t tpw_sinit_off(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
}

int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gRideStatusPanel.mF00));
    out[n++] = static_cast<int64_t>(sizeof(gRideStatusPanel.mF28));
    return n;
}

void tpw_sinit_run() { __sinit_ui_RideStatus_cpp(); }

}  // extern "C"
