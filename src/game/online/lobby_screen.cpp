// lobby_screen.cpp: name proven by __sinit_lobby_screen_cpp in the main application.

#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "FS_Online.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "lobby.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "TbDynamicStringTemplate.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_main.h"
#include "UIInterface.h"
#include <cstddef>
#include <cstdint>
#include <new>

// The lobby's screen system and the base class of its screens (main app of SimTheme Park
// 2.04b3, 0x100b6ba4..0x100b7778, and CLBScreenComponent::IsVisible at 0x100a6fb8). The
// concrete screens (islands, globes, advisor) are untranslated.
//
// The pattern is the lobby's own: a screen adds itself to the screen system in
// gLobbyScreens when it is made and leaves when it is destroyed. The system makes five
// screens in Init and passes the lobby's calls on; Tick, Render, window messages and
// "cancel" go to the visible screens only.

// ---- CLBScreenComponent
CLBScreenComponent::CLBScreenComponent() : mVisible(0) { gLobbyScreens->Register(this); }
CLBScreenComponent::~CLBScreenComponent() { gLobbyScreens->UnRegister(this); }
void CLBScreenComponent::Show() { mVisible = 1; }
void CLBScreenComponent::Hide() { mVisible = 0; }
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

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

// ---- the static initialisers: the wide strings the files hold (Mac 0x1023e9f0 and so on), the objects and the nodes

namespace {
using W = TbDynamicStringTemplate<char16_t>;
const char16_t kScreenW[] = u"upload";             // 0x1023eb56
}  // namespace

alignas(16) unsigned char gV51077ScreenW49bc[sizeof(W)];   // 0x102e49bc
unsigned char gV51077ScreenNode49d0[16];                    // 0x102e49d0
void __sinit_lobby_screen_cpp() {
    void* object = new (static_cast<void*>(gV51077ScreenW49bc)) W(kScreenW);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077ScreenNode49d0);
}
