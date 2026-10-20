// ui_playerLogin.cpp (main app), unit w43: LoginPanelMsg 0x101add48 (92 bytes), EntryMsg 0x101ade94 (744),
// QuitMsg 0x101ae218 (336), RemoveLoginPanel 0x101af02c (528).
#include "lobby.h"
#include "FS_UserInfo.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "TPSettings.h"
#include "TbITable.h"
#include "Text.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

int8_t gLoginHoverRed = -1;   // 0x102ac007
int8_t gLoginHoverGreen = -1; // 0x102ac008
int8_t gLoginHoverBlue;       // 0x10394ac0

namespace {
CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}
}  // namespace

int32_t LoginPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gLoginPanelWindow = nullptr;
    else if (message == 5) {
        RemoveLoginPanel(0);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t EntryMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        window->SetFontColour(gLoginNameRed, gLoginNameGreen, gLoginNameBlue, 0xff);
    } else if (message == 0x10001) {
        window->SetFontColour(gLoginHoverRed, gLoginHoverGreen, gLoginHoverBlue, 0xff);
    } else if (message == 0x10006) {
        const auto slot = static_cast<EFSUserInfoGameUser>(reinterpret_cast<intptr_t>(window->getExtraParam(0)));
        if (UserInfo()->mUsers[slot].mName.Length() == 0) {
            NewName(slot);
        } else {
            UserInfo()->GameLogin(slot);
            RemoveLoginPanel(0);
            if (gPlayerSettings.mCurrent.mTooltipsOn) UI_EnableHelpText();
            else UI_DisableHelpText();
        }
        return 0;
    } else if (message == 0x100) {
        CFSUserInfo* user = UserInfo();
        const auto slot = static_cast<EFSUserInfoGameUser>(reinterpret_cast<intptr_t>(window->getExtraParam(0)));
        const TbStringBase<char16_t>* name = user->GetGameLoginSlotUserName(slot);
        TbUniStringVTag tag(4, name->mData);
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x18f, &tag);
        dlgboxparams params{gUIMainConvertBuffer, 0xff, 0xff, 0xff, 4,
                            static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(0)))};
        UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(1), GameConfirmDelPlayer_OK, dlgbutton(0), nullptr, false);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t QuitMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        window->SetFontColour(gLoginNameRed, gLoginNameGreen, gLoginNameBlue, 0xff);
    } else if (message == 0x10001) {
        window->SetFontColour(0xff, 0, 0, 0xff);
    } else if (message == 0x10006) {
        dlgboxparams params{UIMain_GetString(9), 0xff, 0xff, 0xff, 4, 0};
        UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(1), GameConfirmQuit_OK, dlgbutton(0), nullptr, false);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void RemoveLoginPanel(int32_t cancel) {
    gLoginPanelWindow->GetParent()->CaptureKb(0);
    UI_SendMessage(gLoginPanelWindow, 4, 0, 0);
    gLobby->mWindow->Show(1);
    gLobby->mWindow->CaptureKb(0);
    EnableAllKeytables();
    gLobbyScreens->mAdvisor->ClearAll();
    if (cancel != 0) return;
    if (UserInfo()->mGameUser != -1 && gLoginPanelFirstPlayer != 0) {
        if (TheGameType().mType == kGameTypeEasy) {
            gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x18a}, 1);
        } else {
            if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
            gMasterSave->GiveExtraKey();
            UserInfo()->SaveGlobalMasterSave();
            gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x189}, 1);
        }
    }
    gLobbyScreens->mThemeIslands->ResetCurrentIsland(1);
    gLobbyScreens->mThemeIslands->UpdateSigns();
    UpdateIslandLobbyUI();
}
