// ui_mainmenu.cpp: name proven by __sinit_ui_mainmenu_cpp in the main application.

#define TPW_UIMENU_CTOR_EXTERNAL 1
#include "AdvisorScoring.h"
#include "bfl_FileSystem.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "LbDrawPrimitives.h"
#include "LbSCBack.h"
#include "lobby.h"
#include "online.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "SaveLoad.h"
#include "TbDynamicStringTemplate.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "World.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v5958: the main menu's quit answer, the main menu item callback and its fade, and two of CFSLoadSave's functions (the save
// of a park by name, the first entry of the list of the player's save files).

// 0x101fa8d4 (212): extra parameter 2 is the fade: above 0 the item brightens by 0x20 a frame (grey levels, past 0xff it ends at white
// and the parameter is 0), below 0 it steps down by 0x20 (to 0 once it is below -0xdf) and the item takes its own colour.
void itemOnFrame(InterfaceWindow* window) {
    int32_t fade = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(2)));
    if (fade == 0) return;
    if (fade > 0) {
        int32_t next = fade + 0x20;
        int32_t level = next;
        if (next > 0xff) {
            level = 0xff;
            next = 0;
        }
        window->SetFontColour(level, level, level, 0xff);
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(next)));
    } else {
        int32_t next = fade - 0x20;
        if (fade + 0xdf < 0) next = 0;
        window->SetFontColour(0, 0xaf, 0xbe, 0xff);
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(next)));
    }
}

// 0x101fa9dc (248): the main menu item handler: message 0x1e (a frame) runs the fade, 0x11 sets the item colour, 0x10001 and 0x10002
// start the fade up (1) and down (-1); then the default item handler answers.
int32_t MainMenuItemCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        itemOnFrame(window);
    } else if (message < 0x1e) {
        if (message == 0x11) window->SetFontColour(0, 0xaf, 0xbe, 0xff);
    } else if (message == 0x10002) {
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
    } else if (message < 0x10002 && message > 0x10000) {
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(1)));
    }
    return UIMenu_DefaultItemCallback(window, message, a, b);
}
// 0x101fab10 (148): the quit answer of the game menu's confirm box: the online system (made when there is none) stops its jobs, the game
// goes to the quit state and the main menu, when there is one, is hidden. Answers 1.
int32_t MainGameMenuConfirmQuit_OK(void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes on the Mac
    gOnlineSystem->StopProcessing();
    GS_SetCurrentGameStatus(GS_QUIT_GAME);
    if (gMainMenu != nullptr) HideMainMenu();
    return 1;
}

// ui_mainmenu.cpp (main app), unit v5522: the game and lobby menus' handlers and two dialog answers.
//   MainGameMenuConfirmPublish_OK 0x101fabd8, MainGameMenuCB 0x101fae30, MainLobbyMenuConfirmLogout_OK 0x101fb258,
//   MainLobbyMenuCB 0x101fb354.
// The Mac passes the dialogs' answers as TVectors (the addresses in the comments of ui_mainmenu.h); the host passes the functions.

#define TPW_WEAK __attribute__((weak, noinline))

// ---- the program's definitions of the callees the host cannot call as the Mac does (the test library has fakes, which win)
namespace mainmenu_callee {

TPW_WEAK CFSOnlineSystem* NewOnlineSystem() { return new CFSOnlineSystem(); }

TPW_WEAK CFSUserInfo* NewUserInfo() { return new CFSUserInfo(); }

// 0x10238800: TbDynamicStringTemplate<wchar_t>(const wchar_t*) built in place (the sinit's global is sized for the host string).
TPW_WEAK void WideStringCtor(void* string, const char16_t* text) { new (string) TbDynamicStringTemplate<char16_t>(text); }

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

// Unit v51056: CUIGrapher's setters (0x101fd798 .. 0x101fd97c), CFSLoadSave's Save/Hidden wrappers and destructor, the ui_mainmenu.cpp
// small functions and static initialiser, the TPEmbed.cpp small functions and the bthread / Connection / vector destructors.

alignas(8) uint8_t gMainMenuText[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10396a5c (0x14 bytes on the Mac)
alignas(8) uint8_t gMainMenuTextNode[12];     // 0x10396a70
const char16_t gMainMenuLiteral[] = u"upload";   // 0x1024f694

CFSLoadSave* CFSLoadSave::Destroy(CFSLoadSave* self, int16_t flag) {
    if (self) {
        v51056_callee::LoadSaveListDtor(self);
        if (flag > 0) ftp_callee::OperatorDelete(self);
    }
    return self;
}

int32_t MainGameMenuConfirmRestart_OK(void* context) {
    GS_SetCurrentGameStatus(GS_RESTART_LEVEL);
    HideMainMenu();
    return 1;
}
int32_t BankruptGameMenuConfirmRestart_OK(void* context) {
    GS_SetCurrentGameStatus(GS_RESTART_LEVEL);
    HideMainMenu();
    UI_ByeByeBankrupt();
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

int32_t MainLobbyMenuConfirmQuit_OK(void* context) {
    gLobby->SendQuit();
    return 1;
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

void MainLobbyMenuPostOnlineLoginCallbackFn(int32_t error, const void* context) {}

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

// InitialiseMainMenu 0x101fb760 (1348) and InitialiseLobbyMenu 0x101fbcd0 (864): make the main menu object gMainMenu and its items.

namespace {

// The online system is made when it is not there yet (the original repeats this test before each use).
CFSOnlineSystem* Online() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    return gOnlineSystem;
}

int32_t Add(int32_t position, int32_t stringId, int32_t id, int32_t colour) {
    return gMainMenu->AddItem(position, UIMain_GetString(stringId), id, 0, colour, colour, colour);
}

}  // namespace

void InitialiseMainMenu() {
    gMainMenu = new UIMenu();
    gMainMenu->Init(MainGameMenuCB, MainMenuItemCB);
    const int32_t c = 0x80;
    CGameType& type = TheGameType();   // the guard byte test and the constructor on first use
    if (type.mType == 1) {
        int32_t p = Add(10, 7, 0x10, c);
        p = Add(p + 5, 0xc, 0x13, c);
        Add(p + 5, 8, 0x14, c);
    } else if (gGameWorld->mF1DA738 == 4) {
        int32_t p = Add(10, 3, 0x16, c);
        p = Add(p + 5, 10, 0x17, c);
        p = Add(p + 5, 7, 0x15, c);
        p = Add(p + 5, 0xc, 0x18, c);
        Add(p + 5, 8, 0x19, c);
    } else {
        int32_t n = 5;
        if (Online()->AreWeLoggedIn() != 0) n = 0;
        int32_t p = Add(n << 1, 3, 1, c);
        p = Add(p + n, 4, 2, c);
        p = Add(p + n, 10, 3, c);
        p = Add(p + n, 5, 4, c);
        if (Online()->AreWeLoggedIn() != 0) p = Add(p + n, 2, 7, c);
        if (Online()->AreWeProcessing() == 0) p = Add(p + n, 6, 5, c);
        p = Add(p + n, 7, 0, c);
        p = Add(p + n, 0xc, 6, c);
        Add(p + n, 8, 8, c);
    }
}

void InitialiseLobbyMenu() {
    gMainMenu = new UIMenu();
    gMainMenu->Init(MainLobbyMenuCB, MainMenuItemCB);
    int32_t p = 0;
    if (Online()->mF108 != 0) {
        p = Add(p + 5, 0xe0, 0xd, 0);
        p = Add(p + 5, 7, 0xe, 0);
        Add(p + 5, 8, 0xf, 0);
    } else {
        if (Online()->AreWeLoggedIn() == 0) p = Add(p + 5, 1, 9, 0);
        else p = Add(p + 5, 2, 0xa, 0);
        if (Online()->AreWeProcessing() == 0) p = Add(p + 5, 6, 0xb, 0);
        if (gLobbyScreens->mThemeIslands->IsVisible() != 0) p = Add(p + 5, 0xd, 0xc, 0);
        p = Add(p + 5, 7, 0xe, 0);
        Add(p + 5, 8, 0xf, 0);
    }
}

// Mac file ui_mainmenu.cpp: CreateMainMenu 0x101fc05c (188) and HideMainMenu 0x101fc140 (136).

uint8_t gMainMenuPauseFlag;   // 0x10396a54

// Stand-in for UIMenu::~UIMenu 0x1018ac8c (not translated yet): frees the menu. Weak so a translation or a test fake replaces it.
__attribute__((weak)) void DestroyUIMenu(UIMenu* menu) { delete menu; }

// Makes the main menu (the lobby's when `a` is not 0) and shows it; nothing when it already exists.
void CreateMainMenu(int32_t a) {
    if (gMainMenu != nullptr) return;
    if (a != 0) {
        InitialiseLobbyMenu();
        gMainMenuPauseFlag = 1;
    } else {
        InitialiseMainMenu();
        RemoveSlideoutPanel(1);
        if (gGame.mF3C == 1) {
            gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
            gGame.mF3C = 0;
            gMainMenuPauseFlag = 0;
            UI_PauseTimerSystem();
        } else {
            gMainMenuPauseFlag = 1;
        }
    }
    gMainMenu->Show(a);
}

// Unpauses the game when the menu had paused it, then hides and deletes the menu (no NULL check, as the original).
void HideMainMenu() {
    if (gGame.mF3C == 0 && gMainMenuPauseFlag == 0) {
        gGame.mF3C = 1;
        gGame.Unpause();
        UI_UnPauseTimerSystem();
    }
    gMainMenu->Hide();
    DestroyUIMenu(gMainMenu);
    gMainMenu = nullptr;
}

// ui_main.cpp (main app), unit w49: UIMain_DecideKbCapture 0x1018116c (612 bytes) decides which window has the keyboard after a dialog,
// a panel or the game window has closed. With MainMenuUp 0x101fc1ec (80 bytes; the Mac file ui_mainmenu.cpp) and UIMenu::IsShown
// 0x1018b298 (8 bytes; ui_utils.cpp) and the windows' globals, which the Mac keeps in the data of those files.

extern UIMenu* gMainMenu;

int32_t MainMenuUp() {
    if (gMainMenu != nullptr && gMainMenu->IsShown() != 0) return 1;
    return 0;
}

// ---- ui_mainmenu.cpp
void __sinit_ui_mainmenu_cpp() {
    mainmenu_callee::WideStringCtor(gMainMenuText, gMainMenuLiteral);
    __register_global_object(gMainMenuText, reinterpret_cast<void*>(&UIMain_DestroyWString), gMainMenuTextNode);
}
