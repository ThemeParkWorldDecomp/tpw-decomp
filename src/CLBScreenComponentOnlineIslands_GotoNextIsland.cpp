// The online islands screen's small members (unit v5255): Tick 0x100b3b28, SelectCurrentIsland 0x100b44ac, EnterCurrentIsland
// 0x100b45d0, GotoNextIsland 0x100b46fc, DownloadFailFunction 0x100b4a98, HandleOnlineBacktrack 0x100b4b48.
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "lobby.h"

namespace {

// The word at 0xe8 (mFE8): 1 while the city lobby controller is shown.
inline int32_t GetShown(const CLBScreenComponentOnlineIslands* s) {
    int32_t v;
    std::memcpy(&v, s->mFE8, 4);
    return v;
}
inline void SetShown(CLBScreenComponentOnlineIslands* s, int32_t v) { std::memcpy(s->mFE8, &v, 4); }

}  // namespace

void CLBScreenComponentOnlineIslands::Tick() {
    TickIslands();
    if (mFE4 != 0 && GetShown(this) != 0 && mCurrent != nullptr && mCurrent->mPark != nullptr) UpdateCityLobbyUI(mCurrent);
}

void CLBScreenComponentOnlineIslands::SelectCurrentIsland() {
    if (mCameraState == 0 && mCurrent != nullptr && mCurrent->mPark != nullptr) {
        SetShown(this, 0);
        HideCityLobbyController();
        CreateOnlineParkInfoPanel(mCurrent->mPark, nullptr, IslandOnlineParkInfoPanelCancelCallback,
                                  OnlineIslandParkDownloadCompleteCallback, this);
    }
}

void CLBScreenComponentOnlineIslands::EnterCurrentIslandOnline() {
    gLobby->mState = 2;
    TheGameType().SetGameType(kGameTypeOnline);
}

void CLBScreenComponentOnlineIslands::GotoNextIsland() {
    if (mCameraState == 0) {
        CLBScreenComponentIslands::GotoNextIsland();
        SetShown(this, 1);
        ShowCityLobbyController();
    }
}

void CLBScreenComponentOnlineIslands::DownloadFailFunction() {
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);
        mCameraState = 0;
    }
    ShowCityLobbyController();
}

void CLBScreenComponentOnlineIslands::HandleOnlineBacktrack() {
    Hide();
    gLobbyScreens->mDownloadGlobe->Show();
}
