// The publish globe's window messages, Show, Hide, CreateCities and HandleOnlineBacktrack, and the download globe's
// CreateCities and HandleOnlineBacktrack (unit v5957).
#include <new>
#include <cstring>
#include "lobby.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "tb_resource_display.h"
#include "ui_publishParkPanel.h"

namespace {
void WriteI(uint8_t* p, int32_t v) { std::memcpy(p, &v, 4); }
}  // namespace

// 0x100aeb6c: the base class's messages first; then message 0x10005 with 0 chooses the city of the flag the word at 0x70 holds
// (a Mac pointer to a city flag: its word at 0x0c is the city) and opens the publish panel.
void CLBScreenComponentPublishGlobe::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    CLBScreenComponentOnlineGlobe_WindowMessagesFn(this, message, a, b);
    if (message == 0x10005 && a == 0 && mF70 != 0) {
        CLBGlobeCityFlag* flag =
            reinterpret_cast<CLBGlobeCityFlag*>(tpw::gArenaBase + static_cast<uint32_t>(mF70));   // a 32-bit field: an arena offset (Ptr32.h)
        gOnlineParkSelection = reinterpret_cast<OnlineParkSelection*>(const_cast<CFSOnlineCity*>(flag->mCity));
        CreatePublishParkPanel();
    }
}

// 0x100aec3c
void CLBScreenComponentPublishGlobe::Show() {
    if (IsVisible() == 0) {
        CreateWorldLobbyController(true);
        CreateOnlineCityInfoOverlay();
        CLBScreenComponentOnlineGlobe::Show();
        gLobbyScreens->mAdvisor->ClearAll();
    }
}

// 0x100aece8
void CLBScreenComponentPublishGlobe::Hide() {
    if (IsVisible() == 0) return;
    if (IsVisible() != 0) {
        mVisible = 0;
        RemoveWorldLobbyController();
        while (mGlobes->mCount != 0) {
            CLBGlobe* globe = mGlobes->RemoveHead();
            if (globe != nullptr) delete globe;
        }
        while (mCityFlags->mCount != 0) {
            CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
            if (flag != nullptr) delete flag;
        }
        gLobbyMeshes->DeleteMeshInstanceListEntries();
        gei_Display.mFlags &= 0xfdffffffu;
        SOUND_FadeSoundEffect(mF74);
        SOUND_FadeSoundEffect(mF78);
        mF78 = 0;
        mF74 = 0;
    }
    RemoveOnlineCityInfoOverlay();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->SetPublishing(0, nullptr, nullptr);
}

// 0x100aef90
void CLBScreenComponentPublishGlobe::CreateCities(TbPtrList<CFSOnlineCity>* cities) {
    TbPtrList<CFSOnlineCity>::CNode* node = cities->mHead;
    while (node != nullptr) {
        CFSOnlineCity* city = node->mData;
        node = node->mNext;
        if (IsCityWanted(city) != 0) {
            CLBGlobeCityFlag* flag = new CLBGlobeCityFlag(this, city);
            (void)flag;
        }
    }
    WriteI(mF44 + 0x24, 1);
}

// 0x100af094
void CLBScreenComponentPublishGlobe::HandleOnlineBacktrack() {
    Hide();
    gLobbyScreens->mThemeIslands->Show();
    gLobbyScreens->mThemeIslands->EnterCurrentIsland();
}

// 0x100ae9c0
void CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack() {
    Hide();
    gLobbyScreens->mThemeIslands->Show();
}

// 0x100ae8a0
void CLBScreenComponentDownloadGlobe::CreateCities(TbPtrList<CFSOnlineCity>* cities) {
    TbPtrList<CFSOnlineCity>::CNode* node = cities->mHead;
    while (node != nullptr) {
        CFSOnlineCity* city = node->mData;
        node = node->mNext;
        if (IsCityWanted(city) != 0) {
            CLBGlobeCityFlag* flag = new CLBGlobeCityFlag(this, city);
            (void)flag;
        }
    }
    WriteI(mF44 + 0x24, 1);
    AdvisorResponse response;
    response.mId = 0x256;
    gLobbyScreens->mAdvisor->Add(response, 1);
}
