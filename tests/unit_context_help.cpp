// Test library for the keyboard action wrappers and small panel functions (unit v51122; libtpw_unit_context_help.dylib,
// tests/test_context_help.py), main app:
//   src/game/keyboard.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_mainpanel2.cpp, src/game/ui/ui_optionsPanel.cpp, src/game/ui/ui_parkStatus.cpp
//
// The callees outside the unit (the UIInterface members, CWorld::GetAdvisor, CAdvisor::PlayLastTutorialMessage, UI_SendMessage, the wide
// string constructor and __register_global_object) are recording fakes. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "AdvisorQueue.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "ui_Postcard.h"
#include "ui_parkStatus.h"
#include "FinanceGraphPanelMsg.h"
#include "LoansPanelMsg.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

UIInterface gUIInterface;
CWorld* gGameWorld;
int32_t gGameWindowToggled;
InterfaceWindow* gSetOnOffRoot;
InterfaceWindow* gParkStatusPanel;
InterfaceWindow* gAddressBookPanel;
InterfaceWindow* gLoansPanel;
uint8_t gMainPanel2Text[0x14];
uint8_t gMainPanel2TextNode[12];

#define UI_FAKE(name) void UIInterface::name() { ev("UIInterface::" #name, {lab(this)}); }
UI_FAKE(ShowVersionTag)
UI_FAKE(ToggleHelpText)
UI_FAKE(CamcorderMode)
UI_FAKE(Screen_Research)
UI_FAKE(Screen_PeepLoc)
UI_FAKE(Screen_StaffLoc)
UI_FAKE(ShowMap)
UI_FAKE(Screen_EntryPrice)
UI_FAKE(Screen_StaffCosts)
UI_FAKE(Screen_Loans)
UI_FAKE(Screen_Finance)
UI_FAKE(Screen_AllPeeps)
UI_FAKE(Screen_AllStaff)
UI_FAKE(Screen_AllItems)
UI_FAKE(Screen_ParkStatus)
UI_FAKE(Screen_BuyStaff)

CAdvisor* CWorld::GetAdvisor() {
    ev("GetAdvisor", {lab(this)});
    return reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(ret("GetAdvisor")));
}

int32_t CAdvisor::PlayLastTutorialMessage() {
    ev("PlayLastTutorialMessage", {lab(this)});
    return 0;
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

namespace mainpanel2_callee {
void WideStringCtor(void* string, const char16_t* text) { ev("WideStringCtor", {lab(string)}, {narrow(text).c_str()}); }
}  // namespace mainpanel2_callee

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    (void)destructor;
    ev("__register_global_object", {lab(object), lab(node)});
}

void UIMain_DestroyWString(void*) {}

extern "C" {

uint64_t tpw_ch_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUIInterface", &gUIInterface},
        {"gGameWorld", &gGameWorld},
        {"gGameWindowToggled", &gGameWindowToggled},
        {"gSetOnOffRoot", &gSetOnOffRoot},
        {"gParkStatusPanel", &gParkStatusPanel},
        {"gAddressBookPanel", &gAddressBookPanel},
        {"gLoansPanel", &gLoansPanel},
        {"gMainPanel2Text", gMainPanel2Text},
        {"gMainPanel2TextNode", gMainPanel2TextNode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ch_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ch_layout(int64_t*) { return 0; }

// op: the index into the module's NAMES list.
uint32_t tpw_ch_call(int32_t op, int32_t arg) {
    switch (op) {
    case 0: return static_cast<uint32_t>(context_help());
    case 1: return static_cast<uint32_t>(show_version());
    case 2: return static_cast<uint32_t>(ToggleHelpText());
    case 3: return static_cast<uint32_t>(CamcorderMode());
    case 4: return static_cast<uint32_t>(Screen_Research());
    case 5: return static_cast<uint32_t>(Screen_PeepLoc());
    case 6: return static_cast<uint32_t>(Screen_StaffLoc());
    case 7: return static_cast<uint32_t>(Screen_Map());
    case 8: return static_cast<uint32_t>(Screen_EntryPrice());
    case 9: return static_cast<uint32_t>(Screen_StaffCosts());
    case 10: return static_cast<uint32_t>(Screen_Loans());
    case 11: return static_cast<uint32_t>(Screen_Finance());
    case 12: return static_cast<uint32_t>(Screen_AllPeeps());
    case 13: return static_cast<uint32_t>(Screen_AllStaff());
    case 14: return static_cast<uint32_t>(Screen_AllItems());
    case 15: return static_cast<uint32_t>(Screen_ParkStatus());
    case 16: return static_cast<uint32_t>(Screen_BuyStaff());
    case 17: RemoveLoansPanel(); return 0;
    case 18: MainPanel_EnableResearchButton(arg != 0); return 0;
    case 19: return static_cast<uint32_t>(GameWindowToggle_Hidden());
    case 20: __sinit_ui_mainpanel2_cpp(); return 0;
    case 21: RemoveOptionsPanel(); return 0;
    case 22: RemoveParkStatusPanel(); return 0;
    case 23: RemoveAddressBook(); return 0;
    }
    return 0;
}

}  // extern "C"
