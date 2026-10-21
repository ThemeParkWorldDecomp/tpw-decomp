// Test library for src/game/online/lobby.cpp (libtpw_unit_Lobby.dylib; see tests/test_Lobby.py): the
// lobby object and the base class of its systems, driven by scripts.
//
// Real: CLBLobby, CLBLobbySystem, the list of systems. Fakes that record: the four systems
// the lobby makes (their constructors, destructors and virtual members; the base class's
// registering is real), the screens, the window toolkit, the clock, the sounds, the user
// info and the online system (made on demand), the lobby's own message function.
#include <cstdlib>
#include <cstring>
#include <new>

#include "Clock.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "calllog.h"
#include "outbox_mail.h"
#include "lobby.h"
#include "ui_main.h"

#include "fake_strings.h"

using namespace calllog;

namespace {
constexpr int kSystems = 8;
alignas(16) unsigned char gLobbyStore[sizeof(CLBLobby)];
CLBLobby* gTheLobby;                 // the object in the store while it exists
CLBLobbySystem* gTestSystems[kSystems];
unsigned char gDefinition[8], gBuilt[8], gMail[8], gMessageWindow[8];
alignas(16) unsigned char gUserStore[sizeof(CFSUserInfo)];
alignas(16) unsigned char gOnlineStore[sizeof(CFSOnlineSystem)];

void SetOutboxWord(CFSUserInfo* user, int32_t value) { outbox_mail::Set(user->mOutbox, value); }

// A system of the test: its virtual members record.
class TestSystem : public CLBLobbySystem {
public:
    void Init() override { ev("System.Init", {lab(this)}); }
    void Tick() override { ev("System.Tick", {lab(this)}); }
    void Render() override { ev("System.Render", {lab(this)}); }
    void Close() override { ev("System.Close", {lab(this)}); }
    void WindowMessagesFn(int32_t, int32_t, int32_t) override {}
};
}  // namespace

// ---- globals of other files: the clock is only ever handed to fakes
#ifdef __APPLE__
extern unsigned char gClockStorage[sizeof(CClock)] __asm__("_gClock");
#else
extern unsigned char gClockStorage[sizeof(CClock)] __asm__("gClock");
#endif
alignas(16) unsigned char gClockStorage[sizeof(CClock)] = {};
CFSUserInfo* gUserInfo;
CFSOnlineSystem* gOnlineSystem;
// The lobby's window definition is the interface's root window (one global on the Mac; src/game/ui/UI_Base.cpp defines it).
InterfaceWindow* gUIRootWindow;

int32_t CClock::Read() {
    ev("Clock.Read");
    return static_cast<int32_t>(ret("Read", 1000));
}
int32_t CClock::ReadFixed() {
    ev("Clock.ReadFixed");
    return static_cast<int32_t>(ret("ReadFixed", 1000));
}

// ---- fakes: made on demand
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
CFSUserInfo::CFSUserInfo() {
    ev("CFSUserInfo.ctor", {newlab(this)});
    mGameUser = static_cast<int32_t>(ret("GameUser", -1));
    SetOutboxWord(this, static_cast<int32_t>(ret("Outbox", 0)));
}
CFSOnlineSystem::CFSOnlineSystem() {
    ev("CFSOnlineSystem.ctor", {newlab(this)});
    mF108 = static_cast<int32_t>(ret("Online108", 0));
}

// ---- fakes: the window toolkit, the sounds
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Window.Build", {lab(this), description == gLobbyWindowData, callback == LB_Lobby_WindowMessagesFn});
    return reinterpret_cast<InterfaceWindow*>(gBuilt);
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("Window.CaptureKb", {lab(this), on}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("Window.GetWindow", {lab(this), id});
    return ret("GetWindow", 1) ? reinterpret_cast<InterfaceWindow*>(gMail) : nullptr;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("Window.SetCallbackFunction", {lab(this), callback == LB_Lobby_MailIconMessagesFn});
}
int32_t LB_Lobby_MailIconMessagesFn(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
void UI_CreateVersionNumber(bool show) { ev("UI_CreateVersionNumber", {show}); }
void CreateLoginPanel() { ev("CreateLoginPanel"); }
void SND_RegisterGlobalLobbySounds() { ev("SND_RegisterGlobalLobbySounds"); }
void SND_FreeGlobalLobbySounds() { ev("SND_FreeGlobalLobbySounds"); }
void CLBLobbyGlobals::ParseLobbyDescriptor() { ev("ParseLobbyDescriptor", {newlab(this)}); }
int32_t CLBLobby::WindowMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Lobby.WindowMessagesFn", {lab(this), lab(window), message, a, b});
    return static_cast<int32_t>(ret("WindowMessagesFn", 7));
}

// ---- fakes: the four systems (the base class registers and unregisters them for real)
#define FAKE_SYSTEM(Class, Name)                                                               \
    Class::~Class() { ev(Name ".dtor", {lab(this)}); }                                         \
    void Class::Init() { ev(Name ".Init", {lab(this)}); }                                      \
    void Class::Tick() { ev(Name ".Tick", {lab(this)}); }                                      \
    void Class::Render() { ev(Name ".Render", {lab(this)}); }                                  \
    void Class::Close() { ev(Name ".Close", {lab(this)}); }                                    \
    void Class::WindowMessagesFn(int32_t, int32_t, int32_t) {}
FAKE_SYSTEM(CLBLobbyMeshes, "Meshes")
FAKE_SYSTEM(CLBLobbyView, "View")
FAKE_SYSTEM(CLBFlyingMeshSystem, "Flying")
FAKE_SYSTEM(CLBScreenSystem, "Screens")
CLBLobbyMeshes::CLBLobbyMeshes() { ev("Meshes.ctor", {newlab(this)}); }
void CLBLobbyMeshes::Register(CLBMeshType*) {}
void CLBLobbyMeshes::UnRegister(CLBMeshType*) {}
void CLBLobbyMeshes::Register(CLBMeshInstance*) {}
void CLBLobbyMeshes::UnRegister(CLBMeshInstance*) {}
CLBLobbyView::CLBLobbyView() { ev("View.ctor", {newlab(this)}); }
CLBFlyingMeshSystem::CLBFlyingMeshSystem() { ev("Flying.ctor", {newlab(this)}); }
void CLBFlyingMeshSystem::Register(CLBFlyingMeshGroup*) {}     // (virtual since the flying meshes were translated: the vtable needs them)
void CLBFlyingMeshSystem::UnRegister(CLBFlyingMeshGroup*) {}
// The screen system here is a fake that owns four fake screens (no list, no registering).
CLBScreenSystem::CLBScreenSystem() {
    ev("Screens.ctor", {newlab(this)});
    mThemeIslands = new CLBScreenComponentThemeIslands;
    mOnlineIslands = nullptr;
    mDownloadGlobe = new CLBScreenComponentDownloadGlobe;
    mPublishGlobe = new CLBScreenComponentPublishGlobe;
    mAdvisor = new CLBScreenComponentAdvisor;
    mComponents = nullptr;
}
void CLBScreenSystem::Register(CLBScreenComponent*) {}
void CLBScreenSystem::UnRegister(CLBScreenComponent*) {}
CLBScreenComponent::CLBScreenComponent() : mVisible(0) {}
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return 0; }
#define FAKE_SCREEN(Class, Which)                                                \
    Class::~Class() {}                                                           \
    void Class::Tick() {}                                                        \
    void Class::WindowMessagesFn(int32_t, int32_t, int32_t) {}                   \
    void Class::Show() { ev("Screen.Activate", {Which}); }                       \
    void Class::Hide() {}
CLBScreenComponentIslands::CLBScreenComponentIslands() {}
CLBScreenComponentIslands::~CLBScreenComponentIslands() {}
void CLBScreenComponentIslands::Init() {}
void CLBScreenComponentIslands::Render() {}
void CLBScreenComponentIslands::Close() {}
void CLBScreenComponentIslands::Show() {}
void CLBScreenComponentIslands::Hide() {}
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {}
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentThemeIslands::ProcessCancel() { return 0; }
uint32_t CLBScreenComponentOnlineIslands::ProcessCancel() { return 0; }
uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() { return 0; }
uint32_t CLBScreenComponentAdvisor::ProcessCancel() { return 0; }
FAKE_SCREEN(CLBScreenComponentThemeIslands, 1)
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {}   // (the class's vtable has it; src/game/online/lobby_island.cpp)
FAKE_SCREEN(CLBScreenComponentOnlineIslands, 2)
FAKE_SCREEN(CLBScreenComponentDownloadGlobe, 3)
void CLBScreenComponentDownloadGlobe::Init() {}
FAKE_SCREEN(CLBScreenComponentPublishGlobe, 4)
void CLBScreenComponentPublishGlobe::Init() {}
FAKE_SCREEN(CLBScreenComponentAdvisor, 5)
void CLBScreenComponentAdvisor::Init() {}
void CLBScreenComponentAdvisor::Render() {}
void CLBScreenComponentAdvisor::Close() {}
template class TbTList<AdvisorResponse*, AdvisorResponse*>;

extern "C" {
void* tpw_lb_fixed(int32_t what) {
    switch (what) {
    case 1: return gLobbyStore;
    case 2: return gDefinition;
    case 3: return gBuilt;
    case 4: return gMail;
    case 5: return gUserStore;
    case 6: return gOnlineStore;
    case 7: return gMessageWindow;
    }
    return nullptr;
}
void tpw_lb_begin() {
    gLobby = gTheLobby = nullptr;
    for (auto& system : gTestSystems) system = nullptr;
    gLobbyScreens = nullptr;
    gLobbyFlyingMeshes = nullptr;
    gLobbyView = nullptr;
    gLobbyMeshes = nullptr;
    gLobbyWindowDefinition = nullptr;
    gLobbyReturnFlag = 0;
    gUserInfo = nullptr;
    gOnlineSystem = nullptr;
}

int64_t tpw_lb_op(int32_t op, const int32_t* a) {
    CLBLobby* lobby = gTheLobby;
    CLBLobbySystem*& system = gTestSystems[a[0] & (kSystems - 1)];
    switch (op) {
    case 1:
        std::memset(gLobbyStore, 0xa5, sizeof gLobbyStore);
        gLobby = gTheLobby = new (gLobbyStore) CLBLobby;
        return 0;
    case 2:
    case 7: {   // the lobby goes (7: its list only, which ends its life in this test)
        for (auto& made : gTestSystems)
            if (made != nullptr && lobby->mSystems->Find(made) != nullptr) made = nullptr;
        if (op == 2) lobby->~CLBLobby();
        else lobby->DeleteSystemList();
        const int64_t listGone = lobby->mSystems == nullptr;
        gLobby = gTheLobby = nullptr;
        gLobbyScreens = nullptr;
        gLobbyFlyingMeshes = nullptr;
        gLobbyView = nullptr;
        gLobbyMeshes = nullptr;
        return listGone;
    }
    case 3: return lobby->Init();
    case 4: return lobby->Tick();
    case 5: return lobby->Render();
    case 6: return lobby->Close();
    case 8:
        system = new TestSystem;
        return newlab(system);
    case 9:
        delete system;
        system = nullptr;
        return 0;
    case 10: lobby->Register(system); return 0;
    case 11: lobby->UnRegister(system); return 0;
    case 12: return LB_Lobby_WindowMessagesFn(reinterpret_cast<InterfaceWindow*>(gMessageWindow), a[1], a[2], a[3]);
    case 13: gLobby = a[1] ? gTheLobby : nullptr; return 0;
    case 14:   // what Init finds: the return flag, a window definition, a user (logged in as a[4], outbox a[5]), an online system
        gLobbyReturnFlag = a[1];
        gLobbyWindowDefinition = a[2] ? reinterpret_cast<InterfaceWindow*>(gDefinition) : nullptr;
        gUserInfo = a[3] & 1 ? reinterpret_cast<CFSUserInfo*>(gUserStore) : nullptr;
        if (gUserInfo != nullptr) {
            gUserInfo->mGameUser = a[4];
            SetOutboxWord(gUserInfo, a[5]);
        }
        gOnlineSystem = a[3] & 2 ? reinterpret_cast<CFSOnlineSystem*>(gOnlineStore) : nullptr;
        if (gOnlineSystem != nullptr) gOnlineSystem->mF108 = a[6];
        return 0;
    case 15:
        lobby->mState = a[1];
        lobby->mF1C = a[2];
        return 0;
    }
    return -99;
}

// The state in the order of PpcSide.state in the Python module.
int32_t tpw_lb_state(int64_t* out) {
    int64_t* p = out;
    const CLBLobby* lobby = gTheLobby;
    *p++ = lobby != nullptr;
    if (lobby != nullptr) {
        uint32_t bits;
        std::memcpy(&bits, &lobby->mTickUnits, 4);
        *p++ = lab(lobby->mWindow);
        *p++ = bits;
        *p++ = lobby->mF0C;
        *p++ = lab(lobby->mGlobals);
        *p++ = lobby->mState;
        *p++ = lobby->mSystems != nullptr ? lobby->mSystems->mCount : -1;
        if (lobby->mSystems != nullptr) {
            int32_t n = 0;
            for (const auto* node = lobby->mSystems->mHead; node != nullptr && n < 40; node = node->mNext, ++n) *p++ = lab(node->mData);
        }
        *p++ = lobby->mF1C;
        *p++ = lobby->mWidth;
        *p++ = lobby->mHeight;
        *p++ = lobby->mLastTime;
        *p++ = lobby->mTime;
        *p++ = lobby->mTickTime;
        *p++ = lobby->mStarted != 0xa5a5a5a5u;
    }
    *p++ = gLobby == nullptr ? 0 : gLobby == gTheLobby ? 1 : -1;
    *p++ = lab(gLobbyScreens);
    *p++ = lab(gLobbyFlyingMeshes);
    *p++ = lab(gLobbyView);
    *p++ = lab(gLobbyMeshes);
    *p++ = gLobbyReturnFlag;
    *p++ = lab(gUserInfo);
    *p++ = lab(gOnlineSystem);
    return static_cast<int32_t>(p - out);
}
}
