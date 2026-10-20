// Unit v51106: the online islands' small members and the globes' panel members (lobby screens).
#include <cstdint>
#include <cstring>
#include "FS_Online.h"
#include "lobby.h"
#include "TPSound.h"

void RemoveCityLobbyController();       // 0x101e07f0
void ShowOnlineCityInfoOverlay();       // 0x101e2380
void HideOnlineCityInfoOverlay();       // 0x101e23e8

namespace {
// The Mac word at 0xe8 (mFE8): 1 while the city lobby controller shows.
inline void SetShownOnline(CLBScreenComponentOnlineIslands* s, int32_t v) { std::memcpy(s->mFE8, &v, 4); }

constexpr int32_t kMessageKey = 0x1000b;
constexpr int32_t kMessageClick = 0x10005;
constexpr int32_t kKeyLeft = 0x2500;
constexpr int32_t kKeyRight = 0x2700;
constexpr int32_t kKeyReturn = 0xd;
}  // namespace

// ---- the online islands

// 0x100b4574, slot 0x4c
void CLBScreenComponentOnlineIslands::StartCurrentIslandZoom() { mCameraState = 1; }

// 0x100b4690, slot 0x54
void CLBScreenComponentOnlineIslands::SetCurrentIsland(const CLBIsland* island) {
    if (island == nullptr) return;
    mCurrent = const_cast<CLBIsland*>(island);
    mCameraState = 0;
}

// 0x100b4790, slot 0x44
void CLBScreenComponentOnlineIslands::GotoPreviousIsland() {
    if (mCameraState == 0) {
        CLBScreenComponentIslands::GotoPreviousIsland();
        SetShownOnline(this, 1);
        ShowCityLobbyController();
    }
}

// 0x100b4354, slot 0x34
void CLBScreenComponentOnlineIslands::HidePanels() { HideCityLobbyController(); }

// 0x100b42d4, slot 0x30
void CLBScreenComponentOnlineIslands::ShowPanels() {
    ShowCityLobbyController();
    ShowInfoOverlayPanel();
}

// 0x100b4828
void CLBScreenComponentOnlineIslands::ShowInfoOverlayPanel() { SetShownOnline(this, 1); }

// 0x100b4170: the base class's Hide is inlined (the visible flag, the current island's sounds, then the islands are destroyed).
void CLBScreenComponentOnlineIslands::Hide() {
    if (IsVisible() == 0) return;
    if (IsVisible() != 0) {
        mVisible = 0;
        CLBIsland* current = mCurrent;
        if (current != nullptr) {
            SOUND_FadeSoundEffect(current->mSound1);
            SOUND_FadeSoundEffect(current->mSound2);
            current->mSound2 = 0;
            current->mSound1 = 0;
        }
    }
    while (mIslands->mCount != 0) {
        CLBIsland* island = mIslands->RemoveHead();
        if (island != nullptr) delete island;
    }
    gLobbyMeshes->HideHeightfield();
    gLobbyFlyingMeshes->DeleteFlyingMeshGroupListEntries();
    gLobbyMeshes->DeleteMeshInstanceListEntries();
    HidePanels();
    RemoveCityLobbyController();
}

// 0x100b4be0: the base destructor's text is inlined (the islands are destroyed and the list freed); the program then calls
// CLBScreenComponent's destructor, which the compiler does after this body. The hidden delete flag is the caller's.
CLBScreenComponentOnlineIslands::~CLBScreenComponentOnlineIslands() {
    while (mIslands != nullptr && mIslands->mCount != 0) {   // Lead 2026-10-20: a NULL list is left alone (as src/lobby_globe_close.cpp)
        CLBIsland* island = mIslands->RemoveHead();
        if (island != nullptr) delete island;
    }
    if (mIslands != nullptr) delete mIslands;
    mIslands = nullptr;
}

// 0x100b3bc8
void CLBScreenComponentOnlineIslands::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    if (message == kMessageKey) {
        if (a == kKeyLeft) GotoPreviousIsland();
        else if (a == kKeyRight) GotoNextIsland();
        else if (a == kKeyReturn) SelectCurrentIsland();
    } else if (message == kMessageClick) {
        if (a == 0) SelectCurrentIsland();
    }
}

// ---- the base islands

// 0x100b1e3c: the same text, through the base class's slots.
void CLBScreenComponentIslands::WindowMessagesFnIslands(int32_t message, int32_t a, int32_t b) {
    if (message == kMessageKey) {
        if (a == kKeyLeft) GotoPreviousIsland();
        else if (a == kKeyRight) GotoNextIsland();
        else if (a == kKeyReturn) Slot48();
    } else if (message == kMessageClick) {
        if (a == 0) Slot48();
    }
}

// ---- the globes

// 0x100ae1b0
void CLBScreenComponentOnlineGlobe::Register(CLBGlobeCityFlag* flag) { mCityFlags->AddTail(flag); }

// 0x100aea58
int32_t CLBScreenComponentDownloadGlobe::IsCityValid(const CFSOnlineCity* city) { return city->mValue > 0 ? 1 : 0; }

// 0x100ae828
void CLBScreenComponentDownloadGlobe::HidePanels() {
    HideWorldLobbyController();
    HideOnlineCityInfoOverlay();
    HideWorldLobbySats();
}

// 0x100ae7b0
void CLBScreenComponentDownloadGlobe::ShowPanels() {
    ShowWorldLobbyController();
    ShowOnlineCityInfoOverlay();
    ShowWorldLobbySats();
}

// 0x100ae318
void CLBScreenComponentDownloadGlobe::Tick() { TickGlobe(); }

// 0x100aeb10
void CLBScreenComponentPublishGlobe::Tick() { TickGlobe(); }

// 0x100af148: a city that is not full (its flag is 0 and its value is below the limit at 0x64)
int32_t CLBScreenComponentPublishGlobe::IsCityValid(const CFSOnlineCity* city) {
    if (city->mF60 != 0) return 0;
    return city->mValue < city->mF64 ? 1 : 0;
}

// 0x100aef20
void CLBScreenComponentPublishGlobe::HidePanels() {
    HideWorldLobbyController();
    HideOnlineCityInfoOverlay();
}

// 0x100aeeb0
void CLBScreenComponentPublishGlobe::ShowPanels() {
    ShowWorldLobbyController();
    ShowOnlineCityInfoOverlay();
}

// 0x100af1c8: the base destructor's text is inlined (both lists emptied, destroyed and cleared); CLBScreenComponent's destructor
// follows (the compiler's, after this body).
CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe() {
    while (mGlobes != nullptr && mGlobes->mCount != 0) {   // Lead 2026-10-20: NULL lists are left alone, as src/lobby_globe_close.cpp
        CLBGlobe* globe = mGlobes->RemoveHead();
        if (globe != nullptr) delete globe;
    }
    if (mGlobes != nullptr) delete mGlobes;
    mGlobes = nullptr;
    while (mCityFlags != nullptr && mCityFlags->mCount != 0) {
        CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
        if (flag != nullptr) delete flag;
    }
    if (mCityFlags != nullptr) delete mCityFlags;
    mCityFlags = nullptr;
}
