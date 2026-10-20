// CLBScreenComponentOnlineIslands::CreateParks (0x100b4880), Show (0x100b3ec8) and ProcessCancel (0x100b3cd0).
#include <cstdint>
#include <cstring>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "Game.h"
#include "TPSound.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "lobby.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"

namespace {
// The Mac's "if (gOnlineSystem == 0) make it" that every function here repeats.
CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    return gOnlineSystem;
}
}  // namespace

// 0x100b3cd0: the camera state first (2: the zoom is undone), else the online system when it is busy; the city lobby controller shows
// in every case. Answer 1 when something was cancelled.
uint32_t CLBScreenComponentOnlineIslands::ProcessCancel() {
    bool handled = false;
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);
        handled = true;
        mCameraState = 0;
    }
    if (!handled) {
        if (OnlineSystem()->mF384 == 0) {
            ShowCityLobbyController();
            return 0;
        }
    }
    OnlineSystem()->StopProcessing();
    ShowCityLobbyController();
    return 1;
}

// 0x100b3ec8
void CLBScreenComponentOnlineIslands::Show() {
    if (mainmenu_callee::ScreenIsVisible(this) != 0) return;
    gLobbyScreens->mAdvisor->ClearAll();
    CreateCityLobbyController();
    parkinfo_callee::OnlineIslandsSlot30(this);   // slot 0x30
    if (mainmenu_callee::ScreenIsVisible(this) == 0) {
        mVisible = 1;
        gLobbyMeshes->ShowHeightfield();
        gLobbyView->SetGlobalAmbientLight(1, 0x40404000u);
        uint32_t color = 0xEEEEEE00u;
        gLobbyView->SetGlobalDirectionalLight(1, &color, nullptr, 1);
        gLobbyView->mLightUpOffset = 2.0f;
        gLobbyView->mLightRightOffset = 8.0f;
        gLobbyView->mLightDownward = 1;
        gLobbyView->mFov = gLobby->mGlobals->mIslandFov;
        gLobbyView->SetFogOn();
        gLobbyView->SetFogSettings(0x44DDFFFFu, 50.0f, 250.0f);
        mLast = nullptr;
        mCurrent = nullptr;
        mCameraState = 0;
        OnlineSystem()->StopProcessing();
    }
    mFE4 = 0;
    OnlineSystem()->mParksTop10->GetList(gOnlineParkSelection->mID, PostGetTop10ParksListCallbackFunction, this);
    CLBIsland* island = mCurrent;
    if (island != nullptr) {
        island->mSound1 = SOUND_SendEvent(island->mSound1, gLobbyIslandSounds[island->mSoundSet * 2 + 1], 1, 0, 0, 0);
        island->mSound2 = SOUND_SendEvent(island->mSound2, gLobbyIslandSounds[island->mSoundSet * 2], 2, 0, 0, 0);
        SOUND_Tick();
    }
}

// 0x100b4880: one ParseThemeDescriptor per online park (its theme's name and gate names), then the camera goes to the first island.
// An empty island list would crash the Mac's last lines (the head is read unchecked); so does this.
void CLBScreenComponentOnlineIslands::CreateParks(TbPtrList<CFSOnlinePark>* parks) {
    int32_t order = 0;
    for (POSITION position = parks->GetHeadPosition(); position != nullptr;) {
        CFSOnlinePark* park = parks->GetNext(position);
        TbDynamicStringTemplate<char16_t> themeName;
        {
            TbDynamicStringTemplate<char> narrow(OnlineSystem()->mThemeList->ReturnOnlineThemeName(park->mF4C));
            StringToUniString(themeName, narrow);
        }
        TbDynamicStringTemplate<char16_t> gate1;
        TbDynamicStringTemplate<char16_t> gate2;
        gGame.GetLocalisedThemeGateNames(themeName, gate1, gate2);
        const char16_t* gateText2 = gate2.mData;
        const char16_t* gateText1 = gate1.mData;
        char* theme = OnlineSystem()->mThemeList->ReturnOnlineThemeName(park->mF4C);
        ParseThemeDescriptor(theme, park, order, gateText1, gateText2);
        order++;
    }
    mCurrent = mIslands->mHead->mData;
    mFE4 = 1;
}
