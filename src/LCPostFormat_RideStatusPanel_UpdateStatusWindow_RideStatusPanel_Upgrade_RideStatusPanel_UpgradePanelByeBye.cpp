// Unit v51121: the small functions of ui_ridepanel.cpp (RideStatusPanel, ShopStatusPanel), ui_StaffCostsPanel.cpp, the research panel and
// the tank panel message function.
#include <cstdint>
#include "Debug.h"
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "InterfaceScrollBar.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "TPEngine.h"
#include "World.h"
#include "UI_Base.h"

// Unit v51121: the two words the static initialiser of ui_StaffCostsPanel.cpp sets (names invented).
int32_t gStaffCostsF24;   // 0x10395a24
int32_t gStaffCostsF28;   // 0x10395a28

namespace {

UIStatusPanelFields* Fields(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// The ride preview in the view window at +0x14 (the same body for the ride and the shop panel).
void ShowPreview(void* panel) {
    UIStatusPanelFields* fields = Fields(panel);
    if (fields->mF14.mOffset != 0 && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
        alignas(16) uint8_t memory[sizeof(CoolRect) + 16];
        CoolRect* rect = reinterpret_cast<CoolRect*>(memory);
        kidlist_callee::RectCtor(rect);
        fields->mF14.get()->GetClientRect(rect);
        UIMain_ShowRidePreview(*rect, info->mRideTypeNumber, fields->mF14.get()->mZ + 10);
        kidlist_callee::RectDtor(rect);
    }
}

// The view window's text object (the same body for the ride and the shop panel): the window at +0x18.
void InitView(void* panel) {
    UIStatusPanelFields* fields = Fields(panel);
    InterfaceWindow* window = fields->mF18.get();
    if (window != nullptr) {
        window->setZ(static_cast<int16_t>(window->mZ + 0xb));
        fields->mF18.get()->Show(0);
        void* sprite = lender_callee::OperatorNew(0x19c);
        if (sprite != nullptr) lender_callee::SpriteCtor(sprite);
        fields->mF18.get()->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
        fields->mF18.get()->SetFontJust(1, 1);
        InterfaceFont* font = GetFontTableEntry(8);
        fields->mF18.get()->SetFont(font);
        fields->mF18.get()->SetFontColour(0, 0, 0, 0xff);
        fields->mF18.get()->SetClientDrawText(nullptr);
    }
}

}  // namespace

// 0x101ba930.
void RideStatusPanel::UpgradePanelByeBye() {
    mUpgradeWindow = nullptr;
    reinterpret_cast<UIStatusPanel*>(this)->EnableNameClick(1);
}
void RideStatusPanel::DeInitWindow() {}                               // 0x101baa74
void RideStatusPanel::DeInitStatusWindow() {}                         // 0x101b9a84
void RideStatusPanel::UpdateViewWindow() { ShowPreview(this); }       // 0x101b9264
void RideStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101b9208
void RideStatusPanel::InitViewWindow() { InitView(this); }            // 0x101b90e4
void RideStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101bc1a0

// 0x101bc1f4: the body of Condemned.
void RideStatusPanel::BreakDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    if (mWindow == nullptr) return;
    // 0x1024e487
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    void* thing = gThingArray[id.mIndex].mThing;
    if (thing != nullptr && thing == static_cast<void*>(gStaffRoomPanelThing)) mOps->Slot5();
}

void ShopStatusPanel::InitViewWindow() { InitView(this); }            // 0x101bc468
void ShopStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101bc58c
void ShopStatusPanel::UpdateViewWindow() { ShowPreview(this); }       // 0x101bc5e8
void ShopStatusPanel::DeInitStatusWindow() {}                         // 0x101bce60
void ShopStatusPanel::DeInitWindow() {}                               // 0x101bd6f8
void ShopStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101bdf64

void __sinit_ui_ridepanel_cpp() { gRideListF88 = 0; }                 // 0x101b8c8c

// 0x101bda5c (the name is shared with the staff room's 0x101c25c0, so the host name differs).
int32_t _applytoall_shop(CThing* thing) {
    gShopStatusPanel.applytoallcallback(thing);
    return 1;
}

void RemoveStaffCostsPanel() { UI_SendMessage(gStaffCostsPanel, 4, 0, 0); }   // 0x101bf278
void __sinit_ui_StaffCostsPanel_cpp() {                                       // 0x101bf2dc
    gStaffCostsF28 = 0;
    gStaffCostsF24 = 0x10000;
}
void RemoveResearchPanel() { UI_SendMessage(gResearchPanel, 4, 0, 0); }       // 0x101b5ae8

// 0x101b495c.
int32_t StoreTankMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gResearchEffortBar = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}
