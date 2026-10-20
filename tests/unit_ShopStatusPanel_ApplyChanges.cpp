// Test library for ShopStatusPanel::ApplyChanges, applytoallcallback and Show (unit v5734; libtpw_unit_ShopStatusPanel_ApplyChanges.dylib,
// tests/test_ShopStatusPanel_ApplyChanges.py), main app:
//   src/ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges.cpp
//
// The callees outside the unit are fakes recording their arguments; an object or panel is told apart by the tag word the test writes at +0x100.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "ui_main.h"

using namespace calllog;

CThing* gStaffRoomPanelThing;   // 0x103947a4
CObject* gShopStatusObject;     // 0x103959dc

namespace {
int64_t Tag(const void* p) {
    uint32_t v = 0;
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(p) + 0x100, 4);
    return v;
}
}  // namespace

int32_t ShopStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

void CObject::SetAmountOfSpecialIngredient(int32_t amount) { ev("SetAmount", {Tag(this), amount}); }
void CObject::SetProductQuality(uint8_t quality) { ev("SetQuality", {Tag(this), quality}); }
void CObject::SetPricePerUse(int32_t price) { ev("SetPrice", {Tag(this), price}); }

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t a, int32_t b, int32_t c) {
    ev("OpenWindow", {Tag(this), description == gShopStatusDescription, callback == &ShopStatusMsg, a, b, c});
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int32_t>(message)}); }

extern "C" {

uint64_t tpw_ssp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"shop", &gShopStatusObject},
        {"desc", gShopStatusDescription},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ssp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the panel's quality, ingredient and price fields, and the CObject type halfword.
int32_t tpw_ssp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mQuality));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mIngredient));
    out[n++] = static_cast<int64_t>(offsetof(ShopStatusPanel, mF2c));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    return n;
}

void tpw_ssp_apply(uint64_t panel) { reinterpret_cast<ShopStatusPanel*>(panel)->ApplyChanges(); }
void tpw_ssp_all(uint64_t panel, uint64_t thing) {
    reinterpret_cast<ShopStatusPanel*>(panel)->applytoallcallback(reinterpret_cast<CThing*>(thing));
}
void tpw_ssp_show(uint64_t panel, uint64_t thing) { reinterpret_cast<ShopStatusPanel*>(panel)->Show(reinterpret_cast<CThing*>(thing)); }

}  // extern "C"
