// Test library for the static initialiser of ui_shopStatus.cpp (unit v5716; libtpw_unit_sinit_ui_shopStatus_cpp.dylib,
// tests/test_sinit_ui_shopStatus_cpp.py), main app:
//   src/CreateStaffCostsPanel_UpdateStaffCost_sinit_ui_shopStatus_cpp.cpp   __sinit_ui_shopStatus_cpp
//
// The constructor UIStatusPanel_Construct is a fake recording its object. gShopStatusPanel is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

ShopStatusPanel gShopStatusPanel;

void UIStatusPanel_Construct(UIStatusPanel* self) { ev("UIStatusPanel", {lab(self)}); }

// The shop panel's table: the main app's is in src/ui_StatusPanel_tables.cpp, outside the unit (unit v5L31). This one is a block of the
// arena of its own, made on first use; only its address is compared.
uint32_t* ShopStatusPanelTable() {
    static uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(32 * 4));
    return table;
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gShopStatusPanel},
        {"ops", ShopStatusPanelTable()},
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
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mVtable));
    out[n++] = static_cast<int64_t>(sizeof(gShopStatusPanel.mVtable));
    return n;
}

void tpw_sinit_run() { __sinit_ui_shopStatus_cpp(); }

}  // extern "C"
