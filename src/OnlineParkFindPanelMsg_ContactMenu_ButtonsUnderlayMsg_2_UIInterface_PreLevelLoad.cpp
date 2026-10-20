#include <cstdint>
#include <new>
#include <string>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_tooltip.h"
#include "Debug.h"
#include "MainWindowCallback.h"
#include "UIInterface.h"
#include "World.h"
#include "ui_publishParkPanel.h"
#include "LoadPanelMsg.h"
#include "ui_Postcard.h"
#include "ThingReceivers.h"

// Unit v51061: UIInterface stand-ins, each a thin call into the UI.

void UIInterface::PreLevelLoad() {
    if (GameWindowToggle_Hidden()) ToggleGameWindow();
}

int32_t UIInterface::GetShiftKeyState() { return static_cast<int32_t>(UIMain_GetShiftKeyState()); }

void UIInterface::SetSelectedObject(CObject* object) { UI_SetSelectedObject(object); }

CObject* UIInterface::GetSelectedObject() { return static_cast<CObject*>(UI_GetSelectedObject()); }

CThing* UIInterface::GetSelectedPeep() { return UI_GetSelectedPeep(); }

void UIInterface::Load(void* file) {
    InstantRemoveAllTags();
    LoadTags(file);
}

void UIInterface::MessageTag(CTagParams& params) { AddTag(params); }

void UIInterface::Save(void* file) { SaveTags(file); }

int32_t UIInterface::RemoveLocators() {
    if (StaffPanelUp()) {
        RemoveStaffPanel();
        return 1;
    }
    return 0;
}

void UIInterface::StaffDBChange(int32_t slot) { UI_HirePanelStateChange(slot); }

void UIInterface::ShowGameMainMenu() { CreateMainMenu(0); }

void UIInterface::RideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) { gRideStatusPanel.Condemned(ride); }

void UIInterface::Screen_BuyObject() {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> none;
    none.mIndex = 0;
    CreateRideListPanel(static_cast<UIObjType>(-1), none);
}

void UIInterface::Screen_BuyStaff() { CreateHireListPanel(static_cast<StaffType>(-1)); }

void UIInterface::RideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) { gRideStatusPanel.BreakDown(ride); }

void UIInterface::Screen_ParkStatus() { CreateParkStatusPanel(); }

void UIInterface::RideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>) {}

void UIInterface::Screen_AllItems() { CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr); }

void UIInterface::ResearchCategoryComplete(ResearchCat category) { ResearchPanelCategoryComplete(category); }

void UIInterface::Screen_AllStaff() { CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr); }

void UIInterface::ShowMap() { gUI2DMap.CreateMapPanel(); }

void UIInterface::Screen_AllPeeps() { CreateAllKidsPanel(nullptr); }

void UIInterface::Screen_Finance() { CreateFinancePanel(); }

void UIInterface::Screen_Loans() { CreateLoansPanel(); }

void UIInterface::Screen_StaffCosts() { CreateStaffCostsPanel(); }
