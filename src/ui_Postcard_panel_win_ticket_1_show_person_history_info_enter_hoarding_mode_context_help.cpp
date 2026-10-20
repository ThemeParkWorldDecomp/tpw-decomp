// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.
#include <new>
#include "keytable_actions.h"
#include "analyser.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "intrface.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "AdvisorQueue.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "ui_Postcard.h"
#include "ui_parkStatus.h"
#include "FinanceGraphPanelMsg.h"
#include "LoansPanelMsg.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

// 0x1014f340
int32_t context_help() {
    gGameWorld->GetAdvisor()->PlayLastTutorialMessage();
    return 1;
}

// 0x1014f39c
int32_t show_version() { gUIInterface.ShowVersionTag(); return 1; }
// 0x1014ebf4
int32_t ToggleHelpText() { gUIInterface.ToggleHelpText(); return 1; }
// 0x1014eba4
int32_t CamcorderMode() { gUIInterface.CamcorderMode(); return 1; }
// 0x1014eb50
int32_t Screen_Research() { gUIInterface.Screen_Research(); return 1; }
// 0x1014eafc
int32_t Screen_PeepLoc() { gUIInterface.Screen_PeepLoc(); return 1; }
// 0x1014eaa8
int32_t Screen_StaffLoc() { gUIInterface.Screen_StaffLoc(); return 1; }
// 0x1014ea58
int32_t Screen_Map() { gUIInterface.ShowMap(); return 1; }
// 0x1014ea04
int32_t Screen_EntryPrice() { gUIInterface.Screen_EntryPrice(); return 1; }
// 0x1014e9b0
int32_t Screen_StaffCosts() { gUIInterface.Screen_StaffCosts(); return 1; }
// 0x1014e960
int32_t Screen_Loans() { gUIInterface.Screen_Loans(); return 1; }
// 0x1014e90c
int32_t Screen_Finance() { gUIInterface.Screen_Finance(); return 1; }
// 0x1014e8b8
int32_t Screen_AllPeeps() { gUIInterface.Screen_AllPeeps(); return 1; }
// 0x1014e864
int32_t Screen_AllStaff() { gUIInterface.Screen_AllStaff(); return 1; }
// 0x1014e810
int32_t Screen_AllItems() { gUIInterface.Screen_AllItems(); return 1; }
// 0x1014e7bc
int32_t Screen_ParkStatus() { gUIInterface.Screen_ParkStatus(); return 1; }
// 0x1014e768
int32_t Screen_BuyStaff() { gUIInterface.Screen_BuyStaff(); return 1; }

// 0x101a571c
void RemoveLoansPanel() { UI_SendMessage(gLoansPanel, 4, 0, 0); }
// 0x101ab904
void RemoveOptionsPanel() { UI_SendMessage(gSetOnOffRoot, 4, 0, 0); }
// 0x101ada00
void RemoveParkStatusPanel() { UI_SendMessage(gParkStatusPanel, 4, 0, 0); }
// 0x101b0a84
void RemoveAddressBook() { UI_SendMessage(gAddressBookPanel, 4, 0, 0); }

// 0x101a7830 (16 bytes: blr, the argument is not used; the weak copy in src/UI_pump_UIInterface_CamcorderMode.cpp yields to this one)
void MainPanel_EnableResearchButton(bool) {}

// 0x101a8c1c
int32_t GameWindowToggle_Hidden() { return gGameWindowToggled; }

// 0x101a8df4
const char16_t gMainPanel2Literal[] = u"gms.dat";   // 0x1024e158
void __sinit_ui_mainpanel2_cpp() {
    mainpanel2_callee::WideStringCtor(gMainPanel2Text, gMainPanel2Literal);
    __register_global_object(gMainPanel2Text, reinterpret_cast<void*>(&UIMain_DestroyWString), gMainPanel2TextNode);
}
