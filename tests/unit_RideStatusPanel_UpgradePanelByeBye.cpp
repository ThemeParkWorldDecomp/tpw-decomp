// Test library for the small panel functions (unit v51121; libtpw_unit_RideStatusPanel_UpgradePanelByeBye.dylib,
// tests/test_RideStatusPanel_UpgradePanelByeBye.py), main app:
//   src/game/ui/ui_RideStatus.cpp, src/game/ui/ui_StaffCostsPanel.cpp, src/game/ui/ui_researchPanel.cpp, src/game/ui/ui_ridepanel.cpp, src/game/ui/ui_shopStatus.cpp
//
// The callees outside the unit are fakes that record what the real ones read (windows by the tag word at +0x3f0 of their block) and answer
// from the test's queues; the globals of other files the unit uses are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files
CThing* gStaffRoomPanelThing;
CThingArrayEntry gThingArray[kThingArraySize];
ShopStatusPanel gShopStatusPanel;
InterfaceWindow* gStaffCostsPanel;
InterfaceWindow* gResearchPanel;
InterfaceWindow* gResearchEffortBar;
int16_t gRideListF88;

namespace {

void* gRect;          // the rectangle the unit's constructor fake saw
void* gPanel;         // the panel of the call
RideInfo* gInfo;      // what GetDBInfo answers

int64_t Tag(const void* window) {
    if (window == nullptr) return 0;
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(window) + 0x3f0);
}
int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }

// The panel's table (unit v5L31): the slot at +0x1c records an event.
uint64_t RecSlot(void*, uint32_t offset, const void*) {
    if (offset == 0x1c) ev("Slot5", {1});
    return 0;
}

}  // namespace

// ---- the fakes
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {Tag(this), z}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {Tag(this), on});
    return 0;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {Tag(this), Raw(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {Tag(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {Tag(this), Raw(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {Tag(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {Tag(this), Raw(text)}); }
int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {Tag(this), rect == gRect ? 1 : 0});
    return 0;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {Tag(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(0x1234);
}
namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    return reinterpret_cast<void*>(static_cast<intptr_t>(ret("OperatorNew")));
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {Raw(mem)}); }
}  // namespace lender_callee
namespace kidlist_callee {
void RectCtor(void* rect) {
    gRect = rect;
    ev("RectCtor", {});
}
void RectDtor(void* rect) { ev("RectDtor", {rect == gRect ? 1 : 0}); }
}  // namespace kidlist_callee
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {Raw(this)});
    return gInfo;
}
void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) { ev("UIMain_ShowRidePreview", {&rect == gRect ? 1 : 0, id, z}); }
void UIMain_DestroyRidePreview() { ev("UIMain_DestroyRidePreview", {}); }
void UIStatusPanel::CloseWindow() { ev("CloseWindow", {this == gPanel ? 1 : 0}); }
void UIStatusPanel::EnableNameClick(int32_t enable) { ev("EnableNameClick", {this == gPanel ? 1 : 0, enable}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {Tag(window), message, a, b});
    return 0;
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void ShopStatusPanel::applytoallcallback(CThing* thing) {
    ev("applytoallcallback", {this == &gShopStatusPanel ? 1 : 0, Raw(thing)});
}

extern "C" {

uint64_t tpw_RideStatusPanel_UpgradePanelByeBye_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gStaffRoomPanelThing", &gStaffRoomPanelThing}, {"gThingArray", &gThingArray},
        {"gShopStatusPanel", &gShopStatusPanel},         {"gStaffCostsPanel", &gStaffCostsPanel},
        {"gResearchPanel", &gResearchPanel},             {"gResearchEffortBar", &gResearchEffortBar},
        {"gRideListF88", &gRideListF88},                 {"gStaffCostsF24", &gStaffCostsF24},
        {"gStaffCostsF28", &gStaffCostsF28},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_RideStatusPanel_UpgradePanelByeBye_alloc(uint32_t size) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size)));
}

// The Ptr32 value of a block (its offset from the arena base).
uint64_t tpw_RideStatusPanel_UpgradePanelByeBye_p32(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
}

// A table whose slot at +0x1c records an event (a block of the arena).
uint64_t tpw_RideStatusPanel_UpgradePanelByeBye_makeops() {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(FakeStatusPanelTable<RecSlot>()));
}

// The panel and the ride record of the call.
void tpw_RideStatusPanel_UpgradePanelByeBye_begin(uint64_t panel, uint64_t info) {
    gPanel = reinterpret_cast<void*>(static_cast<uintptr_t>(panel));
    gInfo = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info));
    gRect = nullptr;
}

int32_t tpw_RideStatusPanel_UpgradePanelByeBye_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mUpgradeWindow));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mRideTypeNumber));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    return n;
}

// The functions by the order of the test's table; a is the panel, the window or the thing, b the id or the message.
int32_t tpw_RideStatusPanel_UpgradePanelByeBye_call(int32_t which, uint64_t a, int32_t b, int32_t c, int32_t d) {
    RideStatusPanel* ride = reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(a));
    ShopStatusPanel* shop = reinterpret_cast<ShopStatusPanel*>(static_cast<uintptr_t>(a));
    switch (which) {
        case 0: ride->UpgradePanelByeBye(); break;
        case 1: ride->DeInitWindow(); break;
        case 2: ride->DeInitStatusWindow(); break;
        case 3: ride->UpdateViewWindow(); break;
        case 4: ride->DeInitViewWindow(); break;
        case 5: ride->InitViewWindow(); break;
        case 6: ride->Remove(); break;
        case 7: {
            tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
            id.mIndex = static_cast<uint16_t>(b);
            ride->BreakDown(id);
            break;
        }
        case 8: shop->InitViewWindow(); break;
        case 9: shop->DeInitViewWindow(); break;
        case 10: __sinit_ui_ridepanel_cpp(); break;
        case 11: shop->UpdateViewWindow(); break;
        case 12: shop->DeInitStatusWindow(); break;
        case 13: shop->DeInitWindow(); break;
        case 14: return _applytoall_shop(reinterpret_cast<CThing*>(static_cast<uintptr_t>(a)));
        case 15: shop->Remove(); break;
        case 16: RemoveStaffCostsPanel(); break;
        case 17: __sinit_ui_StaffCostsPanel_cpp(); break;
        case 18: RemoveResearchPanel(); break;
        case 19: return StoreTankMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(a)), b, c, d);
    }
    return 0;
}

}  // extern "C"
