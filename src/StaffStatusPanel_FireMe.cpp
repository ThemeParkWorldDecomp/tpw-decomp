// StaffStatusPanel (unit v5107): FireMe 0x101c5400, Show 0x101c54f0, InitWindow 0x101c4c34, UpdateViewWindow 0x101c423c.
// The panel object has the Mac layout UIStatusPanelFields (ui_main.h).
#include <cstdint>

#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "TPSettings.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "WindowScripts.inc"

int16_t gStaffStatusScript[] = {TPW_WINDOW_SCRIPT_102AEDD0};   // 0x102aedd0: the image's words (generated, devtools/gen_game_tables.py)

void StaffStatusPanel::FireMe() {
    if (gPlayerSettings.mCurrent.mConfirmDeleteOn) {
        dlgboxparams params;
        params.mText = UIMain_GetString(0x18d);
        params.mRed = 0xff;
        params.mGreen = 0xff;
        params.mBlue = 0xff;
        params.mValue8 = 4;
        params.mValueC = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gStaffRoomPanelThing));
        UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), fireme, static_cast<dlgbutton>(0),
                     nullptr, false);
    } else {
        reinterpret_cast<CStaff*>(gStaffRoomPanelThing)->SackMe();
    }
    // The virtual call: table pointer at +0x20, entry 0x34 (an arena offset of a block holding the function).
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    Ptr32<void (*)(StaffStatusPanel*)> entry;
    entry.mOffset = fields->mVtable.get()[0x34 / 4];
    (*entry.get())(this);
}

void StaffStatusPanel::Show(CThing* thing) {
    gStaffStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gStaffStatusScript, StaffStatusMsg, 0x511, 0x512, 0);
    reinterpret_cast<UIStatusPanelFields*>(this)->mF14->SetCallbackFunction(StaffStatusViewWindowCallback);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc0));
}

void StaffStatusPanel::InitWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gStaffStatusThing) < 2) {
        fields->mWindow->GetWindow(0x50d)->Show(0);
        fields->mWindow->GetWindow(0x50f)->Show(0);
    }
    reinterpret_cast<UIStatusPanel*>(this)->InitEditBox(0x520);
}

void StaffStatusPanel::UpdateViewWindow() {
    if (gStaffRoomPanelThing != nullptr) {
        UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
        CoolRect rect;
        fields->mF14->GetClientRect(&rect);
        UIMain_ShowSpritePreview(rect, reinterpret_cast<CPeep*>(gStaffRoomPanelThing), fields->mF14->mZ + 1, 8.0f);
    }
}
