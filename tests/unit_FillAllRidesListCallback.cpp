// Test library for the small panel stand-ins (unit v51109; libtpw_unit_FillAllRidesListCallback.dylib, tests/test_FillAllRidesListCallback.py), main app:
//   src/game/ui/ui_FinancePanel.cpp, src/game/ui/ui_StatusPanel.cpp, src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allKidsPanel.cpp, src/game/ui/ui_allStaffPanel.cpp, src/game/ui/ui_coasterPanel.cpp, src/game/ui/ui_controlPanel.cpp, src/game/ui/ui_entryPricePanel.cpp, src/game/ui/ui_hirePanel.cpp, src/game/ui/ui_loadSavePanel.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_textmanager.cpp   twenty small functions
//
// The callees outside the unit are fakes that record the arguments the real ones read; the globals of other files the unit reads
// are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- the unit's own functions and globals (defined in the src file)
void RemoveAllKidsPanel();
void __sinit_ui_allKidsPanel_cpp();
void __sinit_ui_StatusPanel_cpp();
void __sinit_ui_allStaffPanel_cpp();
void __sinit_ui_textmanager_cpp();
void __sinit_ui_entryPricePanel_cpp();
int32_t FillAllRidesListCallback(CThing* thing);
int32_t StatusPanelIteratorCallback(CThing* thing);
extern uint32_t gStatusPanelWord47a8, gStatusPanelWord47ac, gAllKidsWord47cc, gAllKidsWord47d0, gAllStaffWord47f4, gAllStaffWord47f8;
extern uint8_t gCoolRectDtor[8];
extern uint8_t gCoolRectNode[16];
extern uint8_t gEntryPriceStringStorage[];
extern uint8_t gEntryPriceStringNode[16];

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gAllKidsMsgWindow;
InterfaceWindow* gAllItemsPanel;
InterfaceWindow* gStaffPanel;
InterfaceWindow* gParkScreenWindow;
InterfaceWindow* gFinanceGraphPanel;
InterfaceWindow* gHirePanelWindow;
int32_t gSavePanelOpen;
int32_t gLoadPanelOpen;
InterfaceListControl2* gAllItemsList;
InterfaceWindow* gControlPanel;
InterfaceWindow* gLoansPanel;
int32_t gCoasterEditMode;
InterfaceWindow* gCoasterPanel;
int32_t gStatusPanelListCount;
uint32_t gFinanceGraphFlags;
int32_t gFinanceGraphMode;
CWorld* gGameWorld;
const char16_t* gCheatSignA;

// gUIQueryRect as bytes: the real constructor is a fake, so no vtable is needed.
alignas(16) uint8_t gUIQueryRectRaw[sizeof(CoolRect)] __asm__("_gUIQueryRect");

// what the fakes hand out (set by the test)
static uint8_t* gFakeAnalyser;
static CBankAccount* gFakeBank;
static InterfaceWindow* gFakeWindow;
static uint8_t gUnused;

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {int64_t(lab(window)), message, a, b});
    return 0;
}

void AddItemToAllRidesList(const CThing* thing, InterfaceListControl2* list, CObject* object) {
    ev("AddItemToAllRidesList", {int64_t(lab(thing)), int64_t(lab(list)), int64_t(lab(object))});
}

void WhichCheckButtonsAreDown(uint32_t mask) { ev("WhichCheckButtonsAreDown", {int64_t(mask)}); }

void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n) { ev("LinkSlideoutPanel", {int64_t(lab(panel)), n}); }

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {int64_t(message)}); }

void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("UIMain_CancelAdvisorMessage", {int64_t(message)}); }

void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {int64_t(which)}); }

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {int64_t(lab(this))});
    return reinterpret_cast<CParkAnalyser*>(gFakeAnalyser);
}

void MinAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    ev("MinAnalyserValue", {int64_t(reinterpret_cast<uint8_t*>(history) - gFakeAnalyser), int64_t(value), int64_t(range)});
    value = static_cast<int32_t>(ret("min"));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {int64_t(lab(this))});
    return gFakeBank;
}

bool CBankAccount::PayOffLoan(uint32_t loan) {
    ev("PayOffLoan", {int64_t(lab(this)), int64_t(loan)});
    return ret("PayOffLoan") != 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {int64_t(lab(this)), int64_t(id)});
    return gFakeWindow;
}

void FillLenderList(InterfaceListControl2* list) { ev("FillLenderList", {int64_t(lab(list))}); }

// (the Apple arm64 ABI returns `this` from a constructor, like the Mac's)
extern "C" void* tpw_fr_coolrect_ctor(void* self) __asm__("__ZN8CoolRectC1Ev");
extern "C" void* tpw_fr_coolrect_ctor(void* self) {
    ev("CoolRect", {int64_t(lab(self))});
    return self;
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("__register_global_object", {int64_t(lab(object)), int64_t(lab(destructor)), int64_t(lab(node))});
}

void UIMain_DestroyWString(void* object) { (void)object; }

// the string constructor as a plain function under the constructor's symbol (a real definition would need the vtable)
extern "C" void* tpw_fr_string_ctor(void* self, const char16_t* text) __asm__("__ZN23TbDynamicStringTemplateIDsEC1EPKDs");
extern "C" void* tpw_fr_string_ctor(void* self, const char16_t* text) {
    ev("StringCtor", {int64_t(lab(self))}, {narrow(text).c_str()});
    return self;
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_fr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"allKids", &gAllKidsMsgWindow},     {"allItems", &gAllItemsPanel},
        {"staff", &gStaffPanel},             {"parkScreen", &gParkScreenWindow},
        {"finance", &gFinanceGraphPanel},    {"hire", &gHirePanelWindow},
        {"save", &gSavePanelOpen},           {"load", &gLoadPanelOpen},
        {"itemsList", &gAllItemsList},       {"control", &gControlPanel},
        {"loans", &gLoansPanel},             {"editMode", &gCoasterEditMode},
        {"coaster", &gCoasterPanel},         {"statusCount", &gStatusPanelListCount},
        {"flags", &gFinanceGraphFlags},      {"mode", &gFinanceGraphMode},
        {"world", &gGameWorld},              {"cheat", &gCheatSignA},
        {"w47a8", &gStatusPanelWord47a8},    {"w47ac", &gStatusPanelWord47ac},
        {"w47cc", &gAllKidsWord47cc},        {"w47d0", &gAllKidsWord47d0},
        {"w47f4", &gAllStaffWord47f4},       {"w47f8", &gAllStaffWord47f8},
        {"rect", gUIQueryRectRaw},           {"rectDtor", gCoolRectDtor},
        {"rectNode", gCoolRectNode},         {"string", gEntryPriceStringStorage},
        {"stringNode", gEntryPriceStringNode}, {"stringDtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"fakeAnalyser", &gFakeAnalyser},    {"fakeBank", &gFakeBank},
        {"fakeWindow", &gFakeWindow},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The arena offset of a host address (what the int-typed window globals hold).
uint32_t tpw_fr_offset(uint64_t address) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<const void*>(address))); }

// A zeroed block (32-bit addressable).
uint64_t tpw_fr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// No struct is written by Mac offset.
int32_t tpw_fr_layout(int64_t* out) {
    (void)out;
    return 0;
}

// ---- the functions
uint32_t tpw_fr_FillAllRidesListCallback(uint64_t thing) { return static_cast<uint32_t>(FillAllRidesListCallback(reinterpret_cast<CThing*>(thing))); }
void tpw_fr_sinit_allKids() { __sinit_ui_allKidsPanel_cpp(); }
void tpw_fr_RemoveAllKidsPanel() { RemoveAllKidsPanel(); }
void tpw_fr_sinit_status() { __sinit_ui_StatusPanel_cpp(); }
void tpw_fr_RemoveAllItemsPanel() { RemoveAllItemsPanel(); }
uint32_t tpw_fr_StatusPanelIteratorCallback(uint64_t thing) { return static_cast<uint32_t>(StatusPanelIteratorCallback(reinterpret_cast<CThing*>(thing))); }
void tpw_fr_RemoveAllStaffPanel() { RemoveAllStaffPanel(); }
void tpw_fr_sinit_allStaff() { __sinit_ui_allStaffPanel_cpp(); }
void tpw_fr_sinit_textmanager() { __sinit_ui_textmanager_cpp(); }
void tpw_fr_CoasterButtonsDown(uint32_t mask) { CoasterButtonsDown(mask); }
void tpw_fr_ShowControlPanel() { ShowControlPanel(); }
void tpw_fr_RemoveControlPanel() { RemoveControlPanel(); }
void tpw_fr_RemoveEntryPricePanel() { RemoveEntryPricePanel(); }
void tpw_fr_sinit_entryPrice() { __sinit_ui_entryPricePanel_cpp(); }
uint32_t tpw_fr_GetMinYValue() { return static_cast<uint32_t>(GetMinYValue()); }
void tpw_fr_RemoveFinancePanelPanel() { RemoveFinancePanelPanel(); }
void tpw_fr_RemoveHireListPanel() { RemoveHireListPanel(); }
void tpw_fr_RemoveLoadPanel() { RemoveLoadPanel(); }
void tpw_fr_RemoveSavePanel() { RemoveSavePanel(); }
uint32_t tpw_fr_PaybackLoanOK(uint32_t parameter) {
    return static_cast<uint32_t>(PaybackLoanOK(reinterpret_cast<void*>(static_cast<uintptr_t>(parameter))));
}

}  // extern "C"
