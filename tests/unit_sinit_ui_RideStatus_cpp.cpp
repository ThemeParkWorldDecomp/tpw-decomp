// Test library for the ui_RideStatus.cpp static initialiser (unit v5300; libtpw_unit_sinit_ui_RideStatus_cpp.dylib):
//   src/sinit_ui_RideStatus_cpp.cpp   __sinit_ui_RideStatus_cpp
//
// UIStatusPanel::UIStatusPanel is a recording fake. gRideStatusPanel and gRideStatusPanelOps are defined here.
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
UIStatusPanelOps gRideStatusPanelOps;

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
        {"ops", &gRideStatusPanelOps},
        {"f00", &gRideStatusPanel.mF00},
        {"f28", &gRideStatusPanel.mF28},
        {"mops", &gRideStatusPanel.mOps},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gRideStatusPanel.mF00));
    out[n++] = static_cast<int64_t>(sizeof(gRideStatusPanel.mF28));
    return n;
}

void tpw_sinit_run() { __sinit_ui_RideStatus_cpp(); }

}  // extern "C"
