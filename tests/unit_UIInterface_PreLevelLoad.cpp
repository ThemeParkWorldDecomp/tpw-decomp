// Test library for the UIInterface stand-ins (unit v51061; libtpw_unit_UIInterface_PreLevelLoad.dylib, tests/test_UIInterface_PreLevelLoad.py), main app:
//   src/OnlineParkFindPanelMsg_ContactMenu_ButtonsUnderlayMsg_2_UIInterface_PreLevelLoad.cpp
//
// Every callee is a fake recording the arguments the real one reads and answering from the test's queue; the panels the functions use are
// globals defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "UIInterface.h"
#include "World.h"
#include "LoadPanelMsg.h"
#include "ui_Postcard.h"
#include "ThingReceivers.h"

using namespace calllog;

RideStatusPanel gRideStatusPanel;
CUI2DMap gUI2DMap;

namespace {
void* P(const char* key) { return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret(key)))); }
int64_t W(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }
}  // namespace

int32_t GameWindowToggle_Hidden() {
    ev("GameWindowToggle_Hidden", {});
    return static_cast<int32_t>(ret("GameWindowToggle_Hidden"));
}
void ToggleGameWindow() { ev("ToggleGameWindow", {}); }
uint32_t UIMain_GetShiftKeyState() {
    ev("UIMain_GetShiftKeyState", {});
    return static_cast<uint32_t>(ret("UIMain_GetShiftKeyState"));
}
void UI_SetSelectedObject(void* object) { ev("UI_SetSelectedObject", {W(object)}); }
void* UI_GetSelectedObject() {
    ev("UI_GetSelectedObject", {});
    return P("UI_GetSelectedObject");
}
CThing* UI_GetSelectedPeep() {
    ev("UI_GetSelectedPeep", {});
    return static_cast<CThing*>(P("UI_GetSelectedPeep"));
}
void InstantRemoveAllTags() { ev("InstantRemoveAllTags", {}); }
void LoadTags(void* file) { ev("LoadTags", {W(file)}); }
bool AddTag(CTagParams& params) {
    ev("AddTag", {static_cast<int64_t>(params.mId)});
    return false;
}
void SaveTags(void* file) { ev("SaveTags", {W(file)}); }
int32_t StaffPanelUp() {
    ev("StaffPanelUp", {});
    return static_cast<int32_t>(ret("StaffPanelUp"));
}
void RemoveStaffPanel() { ev("RemoveStaffPanel", {}); }
void UI_HirePanelStateChange(int slot) { ev("UI_HirePanelStateChange", {slot}); }
void CreateMainMenu(int32_t a) { ev("CreateMainMenu", {a}); }
void RideStatusPanel::Condemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("RideStatusPanel::Condemned", {lab(this), static_cast<int64_t>(id.mIndex)});
}
void RideStatusPanel::BreakDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("RideStatusPanel::BreakDown", {lab(this), static_cast<int64_t>(id.mIndex)});
}
void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected) {
    ev("CreateRideListPanel", {static_cast<int32_t>(type), static_cast<int64_t>(selected.mIndex)});
}
void CreateHireListPanel(StaffType type) { ev("CreateHireListPanel", {static_cast<int32_t>(type)}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), W(thing)}); }
void ResearchPanelCategoryComplete(int32_t category) { ev("ResearchPanelCategoryComplete", {category}); }
void CreateAllStaffPanel(StaffType type, CThing* thing) { ev("CreateAllStaffPanel", {static_cast<int32_t>(type), W(thing)}); }
void CUI2DMap::CreateMapPanel() { ev("CUI2DMap::CreateMapPanel", {lab(this)}); }
void CreateAllKidsPanel(CThing* thing) { ev("CreateAllKidsPanel", {W(thing)}); }
void CreateFinancePanel() { ev("CreateFinancePanel", {}); }
void CreateLoansPanel() { ev("CreateLoansPanel", {}); }
void CreateStaffCostsPanel() { ev("CreateStaffCostsPanel", {}); }

extern "C" {

uint64_t tpw_uip_addr(const char* name) {
    if (std::strcmp(name, "rsp") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gRideStatusPanel));
    if (std::strcmp(name, "map") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gUI2DMap));
    return 0;
}

uint64_t tpw_uip_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uip_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTagParams, mId));
    return n;
}

static UIInterface gUi;
static void* Ptr(uint32_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }
static tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> Tag(uint16_t v) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = v;
    return t;
}

uint32_t tpw_uip_pre() { gUi.PreLevelLoad(); return 0; }
uint32_t tpw_uip_shift() { return static_cast<uint32_t>(gUi.GetShiftKeyState()); }
uint32_t tpw_uip_setsel(uint32_t o) { gUi.SetSelectedObject(static_cast<CObject*>(Ptr(o))); return 0; }
uint32_t tpw_uip_getsel() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gUi.GetSelectedObject())); }
uint32_t tpw_uip_getpeep() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gUi.GetSelectedPeep())); }
uint32_t tpw_uip_load(uint32_t f) { gUi.Load(Ptr(f)); return 0; }
uint32_t tpw_uip_msgtag(uint64_t p) { gUi.MessageTag(*reinterpret_cast<CTagParams*>(static_cast<uintptr_t>(p))); return 0; }
uint32_t tpw_uip_save(uint32_t f) { gUi.Save(Ptr(f)); return 0; }
uint32_t tpw_uip_removeloc() { return static_cast<uint32_t>(gUi.RemoveLocators()); }
uint32_t tpw_uip_staffdb(int32_t s) { gUi.StaffDBChange(s); return 0; }
uint32_t tpw_uip_mainmenu() { gUi.ShowGameMainMenu(); return 0; }
uint32_t tpw_uip_condemned(uint16_t t) { gUi.RideCondemned(Tag(t)); return 0; }
uint32_t tpw_uip_buyobj() { gUi.Screen_BuyObject(); return 0; }
uint32_t tpw_uip_buystaff() { gUi.Screen_BuyStaff(); return 0; }
uint32_t tpw_uip_broken(uint16_t t) { gUi.RideBrokenDown(Tag(t)); return 0; }
uint32_t tpw_uip_parkstat() { gUi.Screen_ParkStatus(); return 0; }
uint32_t tpw_uip_deleted(uint16_t t) { gUi.RideDeleted(Tag(t)); return 0; }
uint32_t tpw_uip_allitems() { gUi.Screen_AllItems(); return 0; }
uint32_t tpw_uip_research(int32_t c) { gUi.ResearchCategoryComplete(static_cast<ResearchCat>(c)); return 0; }
uint32_t tpw_uip_allstaff() { gUi.Screen_AllStaff(); return 0; }
uint32_t tpw_uip_showmap() { gUi.ShowMap(); return 0; }
uint32_t tpw_uip_allpeeps() { gUi.Screen_AllPeeps(); return 0; }
uint32_t tpw_uip_finance() { gUi.Screen_Finance(); return 0; }
uint32_t tpw_uip_loans() { gUi.Screen_Loans(); return 0; }
uint32_t tpw_uip_staffcosts() { gUi.Screen_StaffCosts(); return 0; }

}  // extern "C"
