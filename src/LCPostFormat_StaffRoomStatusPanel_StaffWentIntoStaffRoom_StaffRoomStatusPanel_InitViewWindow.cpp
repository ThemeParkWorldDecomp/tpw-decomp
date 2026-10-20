// The staff, show and upgrade status panels' view-window functions and the small free functions around them (unit v51051), main app.
#include <cstdint>
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_controls.h"
#include "ui_main.h"

// The words 0x10395a4c and 0x10395a50 (names invented).
int32_t gStaffPanelWord4c;   // 0x10395a4c
int32_t gStaffPanelWord50;   // 0x10395a50

namespace {
UIStatusPanelFields* Fields(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// The ride preview in the view window at +0x14 (the same body for the show and the upgrade panel).
void UpdateRidePreview(void* panel) {
    UIStatusPanelFields* fields = Fields(panel);
    if (fields->mF14.get() != nullptr && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
        CoolRect rect;
        fields->mF14.get()->GetClientRect(&rect);
        UIMain_ShowRidePreview(rect, info->mRideTypeNumber, fields->mF14.get()->mZ + 10);
    }
}
}  // namespace

// ---- StaffRoomStatusPanel
void StaffRoomStatusPanel::InitViewWindow() {   // 0x101c2d60
    mScript = 0;
    mF28 = -1;
    mF2c = -1;
}
void StaffRoomStatusPanel::UpdateViewWindow() {}     // 0x101c2e44
void StaffRoomStatusPanel::DeInitStatusWindow() {}   // 0x101c3010
void StaffRoomStatusPanel::DeInitWindow() {}         // 0x101c3978
void StaffRoomStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101c4098

// ---- ShowStatusPanel
void ShowStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101c2abc
int32_t _applytoall(CThing* thing) {   // 0x101c25c0
    gShowStatusPanel.applytoallcallback(thing);
    return 1;
}
void ShowStatusPanel::DeInitWindow() {}         // 0x101c218c
void ShowStatusPanel::DeInitStatusWindow() {}   // 0x101c1808
void ShowStatusPanel::UpdateViewWindow() { UpdateRidePreview(this); }   // 0x101c0ea4
void ShowStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101c0e48
void ShowStatusPanel::InitViewWindow() {   // 0x101c0d24
    UIStatusPanelFields* fields = Fields(this);
    InterfaceWindow* window = fields->mF18.get();
    if (window != nullptr) {
        window->setZ(static_cast<int16_t>(window->mZ + 0xb));
        fields->mF18.get()->Show(0);
        void* sprite = statusinit_callee::OperatorNew(0x19c);
        if (sprite != nullptr) statusinit_callee::SpriteCtor(sprite);
        fields->mF18.get()->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
        fields->mF18.get()->SetFontJust(1, 1);
        InterfaceFont* font = GetFontTableEntry(8);
        fields->mF18.get()->SetFont(font);
        fields->mF18.get()->SetFontColour(0, 0, 0, 0xff);
        fields->mF18.get()->SetClientDrawText(nullptr);
    }
}

// ---- StaffStatusPanel
void StaffStatusPanel::InitViewWindow() {}       // 0x101c41a0
void StaffStatusPanel::DeInitViewWindow() { UIMain_DestroySpritePreview(); }   // 0x101c41dc
void StaffStatusPanel::DeInitStatusWindow() {}   // 0x101c4950
void StaffStatusPanel::DeInitWindow() {}         // 0x101c4cf0
void StaffStatusPanel::SetSelected() { UI_SetSelectedPeep(reinterpret_cast<CPeep*>(gStaffRoomPanelThing)); }   // 0x101c5358
void StaffStatusPanel::ChangeName(char16_t* name) {   // 0x101c558c
    if (gStaffRoomPanelThing != nullptr) reinterpret_cast<CStaff*>(gStaffRoomPanelThing)->SetName(name);
}
const char16_t* StaffStatusPanel::GetName() {   // 0x101c55f8
    if (gStaffRoomPanelThing == nullptr) return nullptr;
    return reinterpret_cast<CStaff*>(gStaffRoomPanelThing)->GetName();
}
void StaffStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101c5668
int32_t fireme(void* context) {   // 0x101c53b8
    reinterpret_cast<CStaff*>(context)->SackMe();
    return 1;
}

// ---- UpgradeStatusPanel
void UpgradeStatusPanel::InitViewWindow() {}   // 0x101c5c80
void UpgradeStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101c5cc0
void UpgradeStatusPanel::UpdateViewWindow() { UpdateRidePreview(this); }   // 0x101c5d20

// ---- the staff panel's free functions
void __sinit_ui_staffPanel_cpp() {   // 0x101c0cdc
    gStaffPanelWord50 = 0;
    gStaffPanelWord4c = 0x10000;
}
int32_t StaffPanelUp() { return gStaffPanelState; }   // 0x101c0cac
void RemoveStaffPanel() { RemoveSlideoutPanel(0); }   // 0x101c0c5c
