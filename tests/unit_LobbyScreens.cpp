// Test library for src/game/online/LBScreenComponentAdvisor.cpp, src/game/online/lobby_screen.cpp (libtpw_unit_LobbyScreens.dylib; see
// tests/test_LobbyScreens.py): the lobby's screen system and the base class of its screens,
// driven by scripts.
//
// Real: CLBScreenSystem, CLBScreenComponent, the list of screens. Fakes that record: the
// five concrete screens (constructors, destructors, virtual members, each in the class
// whose vtable has it), the main menu. The lobby system base class does nothing here.
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "lobby.h"

using namespace calllog;

namespace {
constexpr int kScreens = 8;
alignas(16) unsigned char gSystemStore[sizeof(CLBScreenSystem)];
CLBScreenSystem* gSystem;
CLBScreenComponent* gTestScreens[kScreens];

class TestScreen : public CLBScreenComponent {
public:
    void Init() override { ev("Test.Init", {lab(this)}); }
    void Tick() override { ev("Test.Tick", {lab(this)}); }
    void Render() override { ev("Test.Render", {lab(this)}); }
    void Close() override { ev("Test.Close", {lab(this)}); }
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override { ev("Test.WindowMessagesFn", {lab(this), message, a, b}); }
    uint32_t ProcessCancel() override {
        ev("Test.ProcessCancel", {lab(this)});
        return static_cast<uint32_t>(ret("Cancel", 0));
    }
};
}  // namespace

CLBScreenSystem* gLobbyScreens;
CLBLobbySystem::CLBLobbySystem() {}
CLBLobbySystem::~CLBLobbySystem() {}
void CreateMainMenu(int32_t a) { ev("CreateMainMenu", {a}); }

#define RECORD(Class, Name, Member) \
    void Class::Member() { ev(Name "." #Member, {lab(this)}); }
#define RECORD_INPUT(Class, Name) \
    void Class::WindowMessagesFn(int32_t message, int32_t a, int32_t b) { ev(Name ".WindowMessagesFn", {lab(this), message, a, b}); }
#define RECORD_CANCEL(Class, Name)                           \
    uint32_t Class::ProcessCancel() {                        \
        ev(Name ".ProcessCancel", {lab(this)});              \
        return static_cast<uint32_t>(ret("Cancel", 0));      \
    }
#define RECORD_DTOR(Class, Name) \
    Class::~Class() { ev(Name ".dtor", {lab(this)}); }
#define QUIET_SHOW(Class) \
    void Class::Show() {} \
    void Class::Hide() {}

CLBScreenComponentIslands::CLBScreenComponentIslands() { ev("Islands.ctor", {newlab(this)}); }
RECORD_DTOR(CLBScreenComponentIslands, "Islands")
RECORD(CLBScreenComponentIslands, "Islands", Init)
RECORD(CLBScreenComponentIslands, "Islands", Render)
RECORD(CLBScreenComponentIslands, "Islands", Close)
QUIET_SHOW(CLBScreenComponentIslands)
RECORD_DTOR(CLBScreenComponentThemeIslands, "ThemeIslands")
RECORD(CLBScreenComponentThemeIslands, "ThemeIslands", Tick)
RECORD_INPUT(CLBScreenComponentThemeIslands, "ThemeIslands")
RECORD_CANCEL(CLBScreenComponentThemeIslands, "ThemeIslands")
QUIET_SHOW(CLBScreenComponentThemeIslands)
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {}   // (the class's vtable has it; src/game/online/lobby_island.cpp)
RECORD_DTOR(CLBScreenComponentOnlineIslands, "OnlineIslands")
RECORD(CLBScreenComponentOnlineIslands, "OnlineIslands", Tick)
RECORD_INPUT(CLBScreenComponentOnlineIslands, "OnlineIslands")
RECORD_CANCEL(CLBScreenComponentOnlineIslands, "OnlineIslands")
QUIET_SHOW(CLBScreenComponentOnlineIslands)
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() { ev("Globe.ctor", {newlab(this)}); }
RECORD_DTOR(CLBScreenComponentOnlineGlobe, "Globe")
RECORD(CLBScreenComponentOnlineGlobe, "Globe", Render)
RECORD(CLBScreenComponentOnlineGlobe, "Globe", Close)
RECORD_CANCEL(CLBScreenComponentOnlineGlobe, "Globe")
RECORD(CLBScreenComponentOnlineGlobe, "Globe", Tick)  // the globe's own, unit v5571's; its vtable is emitted here
RECORD_INPUT(CLBScreenComponentOnlineGlobe, "Globe")
void CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentDownloadGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack() {}
void CLBScreenComponentPublishGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
void CLBScreenComponentPublishGlobe::HandleOnlineBacktrack() {}
RECORD_DTOR(CLBScreenComponentDownloadGlobe, "DownloadGlobe")
RECORD(CLBScreenComponentDownloadGlobe, "DownloadGlobe", Init)
RECORD(CLBScreenComponentDownloadGlobe, "DownloadGlobe", Tick)
RECORD_INPUT(CLBScreenComponentDownloadGlobe, "DownloadGlobe")
RECORD_CANCEL(CLBScreenComponentDownloadGlobe, "DownloadGlobe")
QUIET_SHOW(CLBScreenComponentDownloadGlobe)
RECORD_DTOR(CLBScreenComponentPublishGlobe, "PublishGlobe")
RECORD(CLBScreenComponentPublishGlobe, "PublishGlobe", Init)
RECORD(CLBScreenComponentPublishGlobe, "PublishGlobe", Tick)
RECORD_INPUT(CLBScreenComponentPublishGlobe, "PublishGlobe")
QUIET_SHOW(CLBScreenComponentPublishGlobe)
RECORD_DTOR(CLBScreenComponentAdvisor, "Advisor")
RECORD(CLBScreenComponentAdvisor, "Advisor", Init)
RECORD(CLBScreenComponentAdvisor, "Advisor", Tick)
RECORD(CLBScreenComponentAdvisor, "Advisor", Render)
RECORD(CLBScreenComponentAdvisor, "Advisor", Close)
RECORD_INPUT(CLBScreenComponentAdvisor, "Advisor")
RECORD_CANCEL(CLBScreenComponentAdvisor, "Advisor")
QUIET_SHOW(CLBScreenComponentAdvisor)
template class TbTList<AdvisorResponse*, AdvisorResponse*>;

extern "C" {
void tpw_ls_begin() {
    gLobbyScreens = gSystem = nullptr;
    for (auto& screen : gTestScreens) screen = nullptr;
}

int64_t tpw_ls_op(int32_t op, const int32_t* a) {
    CLBScreenSystem* system = gSystem;
    CLBScreenComponent*& screen = gTestScreens[a[0] & (kScreens - 1)];
    switch (op) {
    case 1:
        std::memset(gSystemStore, 0xa5, sizeof gSystemStore);
        gLobbyScreens = gSystem = new (gSystemStore) CLBScreenSystem;
        return 0;
    case 2:
    case 9: {   // the system goes (9: its list only, which ends its life in this test)
        for (auto& made : gTestScreens)
            if (made != nullptr && system->mComponents->Find(made) != nullptr) made = nullptr;
        if (op == 2) system->~CLBScreenSystem();
        else system->DeleteScreenSystemList();
        const int64_t listGone = system->mComponents == nullptr;
        gLobbyScreens = gSystem = nullptr;
        return listGone;
    }
    case 3: system->Init(); return 0;
    case 4: system->Tick(); return 0;
    case 5: system->Render(); return 0;
    case 6: system->Close(); return 0;
    case 7: system->WindowMessagesFn(a[1], a[2], a[3]); return 0;
    case 8: system->ProcessCancel(); return 0;
    case 10:
        screen = new TestScreen;
        return newlab(screen);
    case 11:
        delete screen;
        screen = nullptr;
        return 0;
    case 12: system->Register(screen); return 0;
    case 13: system->UnRegister(screen); return 0;
    case 14: {   // the "visible" word of the a[1]-th screen of the list
        int32_t n = 0;
        for (auto* node = system->mComponents->mHead; node != nullptr; node = node->mNext, ++n)
            if (n == a[1]) node->mData->mVisible = a[2];
        return 0;
    }
    case 15:
        if (a[1] == 0) screen->Show();
        else if (a[1] == 1) screen->Hide();
        return screen->IsVisible();
    }
    return -99;
}

// The state in the order of PpcSide.state in the Python module.
int32_t tpw_ls_state(int64_t* out) {
    int64_t* p = out;
    const CLBScreenSystem* system = gSystem;
    *p++ = system != nullptr;
    if (system != nullptr) {
        *p++ = lab(system->mThemeIslands);
        *p++ = lab(system->mOnlineIslands);
        *p++ = lab(system->mDownloadGlobe);
        *p++ = lab(system->mPublishGlobe);
        *p++ = system->mAdvisor == nullptr ? 0 : 1;   // (the advisor is made in place: no label)
        *p++ = system->mComponents != nullptr ? system->mComponents->mCount : -1;
        if (system->mComponents != nullptr) {
            int32_t n = 0;
            for (const auto* node = system->mComponents->mHead; node != nullptr && n < 40; node = node->mNext, ++n) {
                *p++ = lab(node->mData);
                *p++ = node->mData->mVisible;
            }
        }
    }
    for (const CLBScreenComponent* screen : gTestScreens) *p++ = screen != nullptr ? screen->mVisible : -7;
    return static_cast<int32_t>(p - out);
}
}
