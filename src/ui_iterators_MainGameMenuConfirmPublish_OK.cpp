// ui_mainmenu.cpp (main app), unit v5522: the game and lobby menus' handlers and two dialog answers.
//   MainGameMenuConfirmPublish_OK 0x101fabd8, MainGameMenuCB 0x101fae30, MainLobbyMenuConfirmLogout_OK 0x101fb258,
//   MainLobbyMenuCB 0x101fb354.
// The Mac passes the dialogs' answers as TVectors (the addresses in the comments of ui_mainmenu.h); the host passes the functions.
#include <cstdlib>
#include <cstring>
#include <new>

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "SaveLoad.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "World.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_publishParkPanel.h"

#define TPW_WEAK __attribute__((weak, noinline))

// ---- the program's definitions of the callees the host cannot call as the Mac does (the test library has fakes, which win)
namespace mainmenu_callee {

TPW_WEAK CFSOnlineSystem* NewOnlineSystem() { return new CFSOnlineSystem(); }

TPW_WEAK CFSUserInfo* NewUserInfo() { return new CFSUserInfo(); }

// ReturnUploadFilename, UniStringToUploadPath and WideStringDtor (ui_mainmenu.h) have no definition in the program yet: the launcher makes
// stand-ins of them. They would be: a TbDynamicStringTemplate<char16_t> built in place from CFSOnlineSystem::ReturnUploadFilename(); a
// TbDynamicStringTemplate<char> filled by UniStringToString whose text becomes out->mPath; and the string's destructor.

TPW_WEAK void ScreenShow(void* screen) { static_cast<CLBScreenComponent*>(screen)->Show(); }
TPW_WEAK void ScreenHide(void* screen) { static_cast<CLBScreenComponent*>(screen)->Hide(); }
TPW_WEAK int32_t ScreenIsVisible(void* screen) { return static_cast<CLBScreenComponent*>(screen)->IsVisible(); }
// Slot 0x34 of the publish globe's vtable: not declared on the host classes yet (stand-in, nothing happens).
TPW_WEAK void PublishGlobeSlot34(void*) {}

}  // namespace mainmenu_callee

namespace {

// "if (*gOnlineSystem == NULL) *gOnlineSystem = new CFSOnlineSystem": the original repeats it before every use.
CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    return gOnlineSystem;
}

// The yes/no dialog every menu raises: the text of string `id`, white, two buttons (2 without an answer, 1 with `answer`).
void ConfirmDialog(int32_t id, dlgcallback answer) {
    dlgboxparams params;
    params.mText = UIMain_GetString(id);
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    params.mValueC = 0;
    UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), answer, static_cast<dlgbutton>(0), nullptr, false);
}

// The two menus' escape handling: the key 0x1b, or a key the shortcuts table does not know (its answer's low half 0), closes the menu.
bool EscapeCloses(int32_t a, int32_t b) {
    if (a == 0x1b) return true;
    return static_cast<int16_t>(gKeyTableShortcuts->Search(static_cast<int16_t>(a), static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff))) == 0;
}

enum GameAction { kGDefault, kGLoad0, kGLoad1, kGSave0, kGRestart, kGBankruptRestart, kGPublish, kGOptions, kGHide, kGLeave, kGQuit, kGLogout,
                  kGLeaveNoBye, kGQuitNoBye };

// 0x102b14a0: the jump table of the game menu's buttons (a = 0 to 0x19); some entries enter after the UI_ByeByeBankrupt call.
const uint8_t kGameActions[26] = {kGHide, kGLoad0, kGSave0, kGRestart, kGPublish, kGOptions, kGLeaveNoBye, kGLogout, kGQuitNoBye,
                                  kGDefault, kGDefault, kGDefault, kGDefault, kGDefault, kGDefault, kGDefault, kGHide, kGDefault,
                                  kGDefault, kGLeaveNoBye, kGQuitNoBye, kGHide, kGLoad1, kGBankruptRestart, kGLeave, kGQuit};

}  // namespace

// 0x101fabd8 (316)
int32_t MainGameMenuConfirmPublish_OK(void* context) {
    (void)context;
    OnlineSystem();
    alignas(16) uint8_t filename[0x40];   // the Mac's TbDynamicStringTemplate<wchar_t> temporary
    mainmenu_callee::ReturnUploadFilename(filename, gOnlineSystem);
    mainmenu_callee::UniStringToUploadPath(&gOnlineUploadFile, filename);
    mainmenu_callee::WideStringDtor(filename, -1);
    CSaveLoadHeader header;   // the Mac's stack buffer, not filled here
    gSaveLoad.Save(gOnlineUploadFile.mPath, &header, static_cast<CSaveLoad::LoadType>(1));
    GS_SetCurrentGameStatus(GS_LEAVE_PARK);
    uint16_t rides[0x100];
    gGameWorld->ConvertDLRidesUsedToIndices(rides);
    OnlineSystem();
    CFSOnlineSystem* system = gOnlineSystem;
    system->SetPublishing(1, gGame.GetCurrentThemeName().mData, rides);
    HideMainMenu();
    return 1;
}

// 0x101fae30 (904)
int32_t MainGameMenuCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100001) {
        if (static_cast<uint32_t>(a) <= 0x19) {
            switch (kGameActions[a]) {
            case kGLoad0:
                HideMainMenu();
                CreateLoadPanel(0);
                return 0;
            case kGLoad1:
                HideMainMenu();
                CreateLoadPanel(1);
                return 0;
            case kGSave0:
                HideMainMenu();
                CreateSavePanel(0);
                return 0;
            case kGRestart:
                ConfirmDialog(0xb, MainGameMenuConfirmRestart_OK);
                break;
            case kGBankruptRestart:
                ConfirmDialog(0xb, BankruptGameMenuConfirmRestart_OK);
                break;
            case kGPublish:
                ConfirmDialog(0xdd, MainGameMenuConfirmPublish_OK);
                break;
            case kGOptions:
                OnlineSystem();
                if (gOnlineSystem->AreWeProcessing() == 0) {
                    HideMainMenu();
                    CreateOptionsPanel();
                    return 0;
                }
                break;
            case kGHide:
                HideMainMenu();
                return 0;
            case kGLeave:
                UI_ByeByeBankrupt();
                [[fallthrough]];
            case kGLeaveNoBye:
                if (a == 0x13) gLobbyReturnFlag = 1;
                GS_SetCurrentGameStatus(GS_LEAVE_PARK);
                HideMainMenu();
                return 1;
            case kGQuit:
                UI_ByeByeBankrupt();
                [[fallthrough]];
            case kGQuitNoBye:
                ConfirmDialog(9, MainGameMenuConfirmQuit_OK);
                break;
            case kGLogout:
                OnlineSystem();
                gOnlineSystem->Logout();
                HideMainMenu();
                return 1;
            default:
                break;
            }
        }
    } else if (message < 0x100001 && message == 0x1000b) {
        if (EscapeCloses(a, b)) {
            HideMainMenu();
            return 0;
        }
    }
    return UIMenu_DefaultCallback(window, message, a, b);
}

// 0x101fb258 (128)
int32_t MainLobbyMenuConfirmLogout_OK(void* context) {
    (void)context;
    if (gUserInfo == nullptr) gUserInfo = mainmenu_callee::NewUserInfo();
    gUserInfo->GameLogout();
    CreateLoginPanel();
    HideMainMenu();
    return 1;
}

// 0x101fb354 (976)
int32_t MainLobbyMenuCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100001) {
        switch (static_cast<uint32_t>(a) - 9) {   // the jump table at 0x102b1508
        case 0:
            HideMainMenu();
            OnlineSystem();
            gOnlineSystem->Login(MainLobbyMenuPostOnlineLoginCallbackFn, nullptr);
            return 0;
        case 1: {
            HideMainMenu();
            OnlineSystem();
            gOnlineSystem->Logout();
            CLBScreenSystem* screens = gLobbyScreens;
            if (mainmenu_callee::ScreenIsVisible(screens->mDownloadGlobe) != 0) {
                mainmenu_callee::ScreenHide(screens->mDownloadGlobe);
                mainmenu_callee::ScreenShow(screens->mThemeIslands);
                screens->mAdvisor->Add(AdvisorResponse{0x258}, 1);
            } else if (mainmenu_callee::ScreenIsVisible(screens->mOnlineIslands) != 0) {
                mainmenu_callee::ScreenHide(screens->mOnlineIslands);
                mainmenu_callee::ScreenShow(screens->mThemeIslands);
                screens->mAdvisor->Add(AdvisorResponse{0x258}, 1);
            }
            return 0;
        }
        case 2:
            OnlineSystem();
            if (gOnlineSystem->AreWeProcessing() == 0) {
                HideMainMenu();
                CreateOptionsPanel();
                return 0;
            }
            break;
        case 3:
            ConfirmDialog(0xe, MainLobbyMenuConfirmLogout_OK);
            break;
        case 4:
            HideMainMenu();
            OnlineSystem();
            gOnlineSystem->StopProcessing();
            mainmenu_callee::PublishGlobeSlot34(gLobbyScreens->mPublishGlobe);
            return 0;
        case 5:
            HideMainMenu();
            return 0;
        case 6:
            ConfirmDialog(9, MainLobbyMenuConfirmQuit_OK);
            break;
        default:
            break;
        }
    } else if (message < 0x100001 && message == 0x1000b) {
        if (EscapeCloses(a, b)) {
            HideMainMenu();
            return 0;
        }
    }
    return UIMenu_DefaultCallback(window, message, a, b);
}
