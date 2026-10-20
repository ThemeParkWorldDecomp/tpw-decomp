// The lobby's screen system and the base class of its screens (main app of SimTheme Park
// 2.04b3, 0x100b6ba4..0x100b7778, and CLBScreenComponent::IsVisible at 0x100a6fb8). The
// concrete screens (islands, globes, advisor) are untranslated.
//
// The pattern is the lobby's own: a screen adds itself to the screen system in
// gLobbyScreens when it is made and leaves when it is destroyed. The system makes five
// screens in Init and passes the lobby's calls on; Tick, Render, window messages and
// "cancel" go to the visible screens only.
#include "lobby.h"

template class TbTList<CLBScreenComponent*, CLBScreenComponent*>;

// ---- CLBScreenComponent
CLBScreenComponent::CLBScreenComponent() : mVisible(0) { gLobbyScreens->Register(this); }
CLBScreenComponent::~CLBScreenComponent() { gLobbyScreens->UnRegister(this); }
void CLBScreenComponent::Show() { mVisible = 1; }
void CLBScreenComponent::Hide() { mVisible = 0; }
int32_t CLBScreenComponent::IsVisible() { return mVisible; }

// ---- CLBScreenSystem
CLBScreenSystem::CLBScreenSystem() { mComponents = new TbPtrList<CLBScreenComponent>; }

CLBScreenSystem::~CLBScreenSystem() { DeleteScreenSystemList(); }

// The screens are made in this order (it is the order they are ticked in): the theme
// islands, the download globe, the publish globe, the online islands, the advisor.
void CLBScreenSystem::Init() {
    mThemeIslands = new CLBScreenComponentThemeIslands;
    mDownloadGlobe = new CLBScreenComponentDownloadGlobe;
    mPublishGlobe = new CLBScreenComponentPublishGlobe;
    mOnlineIslands = new CLBScreenComponentOnlineIslands;
    mAdvisor = new CLBScreenComponentAdvisor;
    for (POSITION position = mComponents->GetHeadPosition(); position != nullptr;) mComponents->GetNext(position)->Init();
}

void CLBScreenSystem::Tick() {
    for (POSITION position = mComponents->GetHeadPosition(); position != nullptr;) {
        CLBScreenComponent* screen = mComponents->GetNext(position);
        if (screen->IsVisible() != 0) screen->Tick();
    }
}

void CLBScreenSystem::Render() {
    for (POSITION position = mComponents->GetHeadPosition(); position != nullptr;) {
        CLBScreenComponent* screen = mComponents->GetNext(position);
        if (screen->IsVisible() != 0) screen->Render();
    }
}

// Every screen is closed, visible or not; the five go in the reverse of their fields' order.
void CLBScreenSystem::Close() {
    for (POSITION position = mComponents->GetHeadPosition(); position != nullptr;) mComponents->GetNext(position)->Close();
    delete mAdvisor;
    mAdvisor = nullptr;
    delete mPublishGlobe;
    mPublishGlobe = nullptr;
    delete mDownloadGlobe;
    mDownloadGlobe = nullptr;
    delete mOnlineIslands;
    mOnlineIslands = nullptr;
    delete mThemeIslands;
    mThemeIslands = nullptr;
}

// Message 0x1000b with 0x1b (the escape key) is "cancel"; every message then goes to the
// visible screens.
void CLBScreenSystem::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    if (message == 0x1000b && a == 0x1b) ProcessCancel();
    for (POSITION position = mComponents->GetHeadPosition(); position != nullptr;) {
        CLBScreenComponent* screen = mComponents->GetNext(position);
        if (screen->IsVisible() != 0) screen->WindowMessagesFn(message, a, b);
    }
}

void CLBScreenSystem::ProcessCancel() {
    uint32_t taken = 0;
    for (POSITION position = mComponents->GetHeadPosition(); position != nullptr;) {
        CLBScreenComponent* screen = mComponents->GetNext(position);
        if (screen->IsVisible() != 0) taken |= screen->ProcessCancel();
    }
    if (taken == 0) CreateMainMenu(1);
}

void CLBScreenSystem::DeleteScreenSystemList() {
    while (mComponents->mCount != 0) {
        CLBScreenComponent* screen = mComponents->RemoveHead();
        if (screen != nullptr) delete screen;
    }
    delete mComponents;
    mComponents = nullptr;
}

void CLBScreenSystem::Register(CLBScreenComponent* screen) { mComponents->AddTail(screen); }

void CLBScreenSystem::UnRegister(CLBScreenComponent* screen) {
    POSITION position = mComponents->Find(screen);
    if (position != nullptr) mComponents->RemoveAt(position);
}
