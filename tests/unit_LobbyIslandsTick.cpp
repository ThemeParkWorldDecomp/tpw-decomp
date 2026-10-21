// Test library for src/game/online/LBFlyingMesh.cpp, src/game/online/lobby_island.cpp (libtpw_unit_LobbyIslandsTick.dylib; see
// tests/test_LobbyIslandsTick.py): the islands screen's per-frame work, driven by scripts.
//
// Real: CLBScreenComponentIslands::TickIslands, CalcCamera, GotoNextIsland and
// GotoPreviousIsland, CLBScreenComponentThemeIslands::Tick, CLBFlyingMeshSystem::Init, Tick and
// Render, and the list of islands (TbTList's Find). Fakes that record: everything the code
// calls (the weather, the sounds, the meshes, the lobby's random numbers, the advisor, the
// flying mesh groups, the user info that is made on demand, the screen's virtual SetCurrentIsland
// and EnterCurrentIsland). The lobby, the lobby view, the screen system and the island meshes are
// plain storage that only the code under test reads and writes.
#include <cstdint>
#include <cstring>
#include <new>

#include "FS_UserInfo.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/msl.h"
#include "tb_rain.h"

#include "fake_strings.h"   // the user info holds strings; its constructor is a fake here

using namespace calllog;

// The sound wrappers, declared as the translation declares them (they answer a handle).
int32_t SOUND_SendEvent(int32_t handle, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c);
void SOUND_Tick();

namespace {
constexpr int kIslands = 6;
constexpr int kBankSets = 4;

// Labels of the objects the test names (the PowerPC side gives its objects the same).
constexpr int kLabelScreen = 1, kLabelView = 2, kLabelLobby = 3, kLabelRain = 4, kLabelAdvisor = 5;
constexpr int kLabelFlying = 6, kLabelMesh = 10, kLabelIsland = 30, kLabelGroup = 40;

alignas(16) unsigned char gLobbyStore[sizeof(CLBLobby)];
alignas(16) unsigned char gGlobalsStore[sizeof(CLBLobbyGlobals)];
alignas(16) unsigned char gViewStore[sizeof(CLBLobbyView)];
alignas(16) unsigned char gScreensStore[sizeof(CLBScreenSystem)];
alignas(16) unsigned char gRainStore[sizeof(CRain)];
alignas(16) unsigned char gAdvisorStore[16];
alignas(16) unsigned char gMeshStore[2 * kIslands][16];
int32_t gBanks[kBankSets][2];
bool gSetsCurrent;   // whether the fake SetCurrentIsland does what the real ones do

// The screen under test: the theme islands, whose virtual members the test records.
class TestIslands : public CLBScreenComponentThemeIslands {
public:
    void EnterCurrentIsland() override { ev("Screen.EnterCurrentIsland", {lab(this)}); }
};
TestIslands* gScreen;
alignas(16) uint8_t gIslandStore[kIslands][sizeof(CLBIsland)];
CLBIsland* gIslands[kIslands];
CLBFlyingMeshSystem* gFlying;
CLBFlyingMeshGroup* gGroups[4];

float Float(uint32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}
uint32_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}
// A float's bits with every NaN the same.
uint32_t Canon(uint32_t bits) { return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0 ? 0x7fc00000u : bits; }
uint32_t Canon(float value) { return Canon(Bits(value)); }
}  // namespace

// ---- globals of the code under test
CLBLobby* gLobby;
CLBLobbyView* gLobbyView;
CLBScreenSystem* gLobbyScreens;
CRain* gei_Rain;
CFSUserInfo* gUserInfo;
int32_t gLobbyThunderBank;
int32_t* gLobbyIslandSounds;

// ---- fakes: the user info, made on demand
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
CFSUserInfo::CFSUserInfo() {
    ev("UserInfo.ctor", {newlab(this)});
    mGameUser = static_cast<int32_t>(ret("GameUser", -1));
}

// ---- fakes: the lobby's random numbers, the C library's, the sounds
uint32_t CLBLobby::GetRand() {
    ev("Lobby.GetRand");
    return static_cast<uint32_t>(ret("GetRand", 0x12345678));
}
int32_t msl_rand() {
    ev("rand");
    return static_cast<int32_t>(ret("rand", 0));
}
int32_t SOUND_SendEvent(int32_t handle, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("Sound.Send", {handle, bank, event, a, b, c});
    return static_cast<int32_t>(ret("Sound", 1000));
}
void SOUND_FadeSoundEffect(int32_t handle) { ev("Sound.Fade", {handle}); }
void SOUND_Tick() { ev("Sound.Tick"); }

// ---- fakes: the weather
void CRain::SetRainType(int32_t type) { ev("Rain.SetRainType", {lab(this), type}); }
void CRain::SetRaindrops(int32_t count) { ev("Rain.SetRaindrops", {lab(this), count}); }
void CRain::LightningFlash() { ev("Rain.LightningFlash", {lab(this)}); }
int32_t CRain::AddLightning(D3DVECTOR start, D3DVECTOR end, int32_t a, int32_t b) {
    ev("Rain.AddLightning", {lab(this), Canon(start.x), Canon(start.y), Canon(start.z), Canon(end.x), Canon(end.y), Canon(end.z), a, b});
    return 1;
}

// ---- fakes: the meshes, the advisor, the flying mesh groups
void CLBMeshInstance::PlayAnim(int32_t anim, int32_t loop) { ev("Mesh.PlayAnim", {lab(this), anim, loop}); }
uint32_t CLBMeshInstance::GetAnimTimeRemaining() {
    ev("Mesh.AnimTime", {lab(this)});
    return static_cast<uint32_t>(ret("Anim", lab(this), 0));
}
void CLBScreenComponentAdvisor::SetTimeoutResponse(AdvisorResponse response, uint32_t milliseconds) {
    ev("Advisor.SetTimeoutResponse", {lab(this), response.mId, milliseconds});
}
void CLBFlyingMeshGroup::Tick() { ev("Group.Tick", {lab(this)}); }

// ---- fakes: the classes the objects under test sit on (nothing in them is called)
CLBLobbySystem::CLBLobbySystem() {}
CLBLobbySystem::~CLBLobbySystem() {}
CLBFlyingMeshSystem::CLBFlyingMeshSystem() : mGroups(nullptr) {}
CLBFlyingMeshSystem::~CLBFlyingMeshSystem() {}
void CLBFlyingMeshSystem::Register(CLBFlyingMeshGroup*) {}     // (virtual since the flying meshes were translated: the vtable needs them)
void CLBFlyingMeshSystem::UnRegister(CLBFlyingMeshGroup*) {}
void CLBFlyingMeshSystem::Close() {}
void CLBFlyingMeshSystem::WindowMessagesFn(int32_t, int32_t, int32_t) {}
CLBScreenComponent::CLBScreenComponent() : mVisible(0) {}
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return 0; }
CLBScreenComponentIslands::CLBScreenComponentIslands() {}
CLBScreenComponentIslands::~CLBScreenComponentIslands() {}
void CLBScreenComponentIslands::Init() {}
void CLBScreenComponentIslands::Render() {}
void CLBScreenComponentIslands::Close() {}
void CLBScreenComponentIslands::Show() {}   // the base vtable's slots 0x24 and 0x28 (translated in src/game/online/lobby_island.cpp, not linked here)
void CLBScreenComponentIslands::Hide() {}
// Slot 0x54, faked on both sides (the base's 0x100b25d0 is translated in src/game/online/lobby_island.cpp,
// which this unit does not link; the theme islands' 0x100b3540 is not translated).
void CLBScreenComponentIslands::SetCurrentIsland(const CLBIsland* island) {
    ev("Screen.SetCurrentIsland", {lab(this), lab(island)});
    if (gSetsCurrent) mCurrent = const_cast<CLBIsland*>(island);
}
CLBScreenComponentThemeIslands::~CLBScreenComponentThemeIslands() {}
void CLBScreenComponentThemeIslands::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentThemeIslands::ProcessCancel() { return 0; }
// (The tests' screen overrides EnterCurrentIsland; the theme class's own is src/game/online/lobby_island.cpp's.)
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {}
void CLBScreenComponentThemeIslands::Show() {}
void CLBScreenComponentThemeIslands::Hide() {}

namespace {
// The words of a case are read in the order the Python module writes them.
struct Reader {
    const uint32_t* cursor;
    uint32_t word() { return *cursor++; }
    int32_t sword() { return static_cast<int32_t>(*cursor++); }
    float real() { return Float(*cursor++); }
    void point(s3DPoint& p) {
        p.x = real();
        p.y = real();
        p.z = real();
    }
};

// The words of the screen from 0x10 to 0xe4 in the Mac's order.
void ReadScreen(Reader& in, CLBScreenComponentIslands& s) {
    s.mLast = reinterpret_cast<CLBIsland*>(static_cast<uintptr_t>(static_cast<uint32_t>(in.sword())));
    s.mCameraState = in.sword();
    s.mAngle = in.real();
    s.mSpinSpeed = in.real();
    s.mRadius = in.real();
    s.mHeight = in.real();
    in.point(s.mCameraPos);
    in.point(s.mCameraGoal);
    in.point(s.mLookAt);
    in.point(s.mLookAtGoal);
    in.point(s.mRoamCentre);
    in.point(s.mRoamExtent);
    s.mRoamSpeed = in.real();
    s.mRoamArriveDistSq = in.real();
    in.point(s.mRoamPos);
    in.point(s.mRoamGoal);
    in.point(s.mRoamDir);
    in.point(s.mRoamGoalDir);
    s.mLookMaxSpeed = in.real();
    s.mLookSpeed = in.real();
    s.mLookSlowDistSq = in.real();
    in.point(s.mLookPos);
    in.point(s.mLookGoal);
    in.point(s.mLookDir);
    in.point(s.mLookGoalDir);
    std::memcpy(&s.mFE4, &*in.cursor, 4);
    ++in.cursor;
}

void Point(uint32_t*& out, const s3DPoint& p) {
    *out++ = Canon(p.x);
    *out++ = Canon(p.y);
    *out++ = Canon(p.z);
}

void WriteScreen(uint32_t*& out, const CLBScreenComponentIslands& s) {
    *out++ = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mLast));
    *out++ = static_cast<uint32_t>(s.mCameraState);
    *out++ = Canon(s.mAngle);
    *out++ = Canon(s.mSpinSpeed);
    *out++ = Canon(s.mRadius);
    *out++ = Canon(s.mHeight);
    Point(out, s.mCameraPos);
    Point(out, s.mCameraGoal);
    Point(out, s.mLookAt);
    Point(out, s.mLookAtGoal);
    Point(out, s.mRoamCentre);
    Point(out, s.mRoamExtent);
    *out++ = Canon(s.mRoamSpeed);
    *out++ = Canon(s.mRoamArriveDistSq);
    Point(out, s.mRoamPos);
    Point(out, s.mRoamGoal);
    Point(out, s.mRoamDir);
    Point(out, s.mRoamGoalDir);
    *out++ = Canon(s.mLookMaxSpeed);
    *out++ = Canon(s.mLookSpeed);
    *out++ = Canon(s.mLookSlowDistSq);
    Point(out, s.mLookPos);
    Point(out, s.mLookGoal);
    Point(out, s.mLookDir);
    Point(out, s.mLookGoalDir);
    uint32_t last;
    std::memcpy(&last, &s.mFE4, 4);
    *out++ = last;
}

void Release() {
    for (auto*& island : gIslands) {
        island = nullptr;
    }
    if (gScreen != nullptr) {
        delete gScreen->mIslands;
        delete gScreen;
        gScreen = nullptr;
    }
    ::operator delete(gUserInfo);   // (made by the code under test or by the test; its destructor is not part of the unit)
    gUserInfo = nullptr;
}
}  // namespace

extern "C" {
// Makes the world a case describes: the lobby, the view, the sounds, the user, the screen and
// its islands. The order of the words is the Python module's (world_words).
void tpw_li_begin(const uint32_t* words) {
    Release();
    Reader in{words};
    const int32_t count = in.sword();
    const int32_t listCount = in.sword();   // the count word the list is left with
    const int32_t current = in.sword();
    const uint32_t userPresent = in.word();
    const int32_t gameUser = in.sword();
    std::memset(gLobbyStore, 0xa5, sizeof gLobbyStore);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyStore);
    gLobby->mStarted = in.word();
    gLobby->mTickUnits = in.real();
    gLobby->mGlobals = reinterpret_cast<CLBLobbyGlobals*>(gGlobalsStore);
    gLobby->mGlobals->mSpinSpeed = in.real();
    gLobby->mGlobals->mSpinRadius = in.real();
    gLobby->mGlobals->mVerticalOffset = in.real();
    gLobbyThunderBank = in.sword();
    for (auto& set : gBanks)
        for (auto& bank : set) bank = in.sword();
    gLobbyIslandSounds = &gBanks[0][0];
    gSetsCurrent = in.word() != 0;
    std::memset(gViewStore, 0xa5, sizeof gViewStore);
    gLobbyView = reinterpret_cast<CLBLobbyView*>(gViewStore);
    in.point(gLobbyView->mPosition);
    in.point(gLobbyView->mTarget);
    for (auto& colour : gLobbyView->mSkyTarget) colour = in.sword();
    for (auto& colour : gLobbyView->mSkyCurrent) colour = in.sword();
    gLobbyView->mFadeCurrent = in.sword();
    gLobbyView->mFadeTarget = in.sword();
    std::memset(gScreensStore, 0xa5, sizeof gScreensStore);
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(gScreensStore);
    gLobbyScreens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(gAdvisorStore);
    gei_Rain = reinterpret_cast<CRain*>(gRainStore);

    tpw_cl_label(kLabelView, gLobbyView);
    tpw_cl_label(kLabelLobby, gLobby);
    tpw_cl_label(kLabelRain, gei_Rain);
    tpw_cl_label(kLabelAdvisor, gAdvisorStore);

    gScreen = new TestIslands;
    tpw_cl_label(kLabelScreen, gScreen);
    ReadScreen(in, *gScreen);
    gScreen->mIslands = new TbPtrList<CLBIsland>;
    for (int32_t i = 0; i < count; ++i) {
        CLBIsland* island = gIslands[i] = reinterpret_cast<CLBIsland*>(gIslandStore[i]);
        std::memset(gIslandStore[i], 0, sizeof gIslandStore[i]);
        tpw_cl_label(kLabelIsland + i, island);
        island->mMeshA = reinterpret_cast<CLBMeshInstance*>(gMeshStore[2 * i]);
        island->mMeshB = reinterpret_cast<CLBMeshInstance*>(gMeshStore[2 * i + 1]);
        tpw_cl_label(kLabelMesh + 2 * i, island->mMeshA);
        tpw_cl_label(kLabelMesh + 2 * i + 1, island->mMeshB);
        island->mAngle = in.real();
        island->mSoundSet = in.sword();
        island->mSound1 = in.sword();
        island->mSound2 = in.sword();
        in.point(island->mPosition);
        island->mLightning = in.sword();
        island->mRain = in.sword();
        for (auto& colour : island->mSky) colour = in.sword();
        gScreen->mIslands->AddTail(island);
    }
    gScreen->mIslands->mCount = listCount;
    gScreen->mCurrent = current >= 0 ? gIslands[current] : nullptr;
    if (userPresent != 0) {
        gUserInfo = static_cast<CFSUserInfo*>(::operator new(sizeof(CFSUserInfo)));
        gUserInfo->mGameUser = gameUser;
        tpw_cl_label(60, gUserInfo);
    }
}

// 1: TickIslands, 2: CalcCamera, 3: GotoNextIsland, 4: GotoPreviousIsland, 5: the theme
// islands' Tick.
void tpw_li_op(int32_t op) {
    switch (op) {
    case 1: gScreen->TickIslands(); break;
    case 2: gScreen->CalcCamera(); break;
    // (The base class's, as the unit tests them; the theme islands' own are src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp, src/game/online/LBFlyingMesh.cpp's.)
    case 3: gScreen->CLBScreenComponentIslands::GotoNextIsland(); break;
    case 4: gScreen->CLBScreenComponentIslands::GotoPreviousIsland(); break;
    case 5: gScreen->Tick(); break;
    }
}

// The state in the order of Side.state in the Python module.
int32_t tpw_li_state(uint32_t* out) {
    uint32_t* p = out;
    *p++ = static_cast<uint32_t>(lab(gScreen->mCurrent));
    WriteScreen(p, *gScreen);
    Point(p, gLobbyView->mPosition);
    Point(p, gLobbyView->mTarget);
    for (const auto colour : gLobbyView->mSkyTarget) *p++ = static_cast<uint32_t>(colour);
    for (const auto colour : gLobbyView->mSkyCurrent) *p++ = static_cast<uint32_t>(colour);
    *p++ = static_cast<uint32_t>(gLobbyView->mFadeCurrent);
    *p++ = static_cast<uint32_t>(gLobbyView->mFadeTarget);
    *p++ = gLobby->mStarted;
    *p++ = Canon(gLobby->mTickUnits);
    *p++ = gUserInfo != nullptr ? 1u : 0u;
    *p++ = gUserInfo != nullptr ? static_cast<uint32_t>(gUserInfo->mGameUser) : 0u;
    for (const CLBIsland* island : gIslands) {
        if (island == nullptr) continue;
        *p++ = Canon(island->mAngle);
        *p++ = static_cast<uint32_t>(island->mSoundSet);
        *p++ = static_cast<uint32_t>(island->mSound1);
        *p++ = static_cast<uint32_t>(island->mSound2);
        Point(p, island->mPosition);
        *p++ = static_cast<uint32_t>(island->mLightning);
        *p++ = static_cast<uint32_t>(island->mRain);
        for (const auto colour : island->mSky) *p++ = static_cast<uint32_t>(colour);
    }
    return static_cast<int32_t>(p - out);
}

// ---- the flying meshes: a system over `count` groups
void tpw_li_flying_begin(int32_t count) {
    delete gFlying;
    gFlying = nullptr;
    gFlying = new CLBFlyingMeshSystem;
    tpw_cl_label(kLabelFlying, gFlying);
    gFlying->mGroups = new TbPtrList<CLBFlyingMeshGroup>;
    for (int32_t i = 0; i < count; ++i) {
        gGroups[i] = static_cast<CLBFlyingMeshGroup*>(::operator new(sizeof(CLBFlyingMeshGroup)));   // (its constructor is the game's, untranslated)
        tpw_cl_label(kLabelGroup + i, gGroups[i]);
        gFlying->mGroups->AddTail(gGroups[i]);
    }
}

// 6: Init, 7: Tick, 8: Render.
void tpw_li_flying_op(int32_t op) {
    switch (op) {
    case 6: gFlying->Init(); break;
    case 7: gFlying->Tick(); break;
    case 8: gFlying->Render(); break;
    }
}

int32_t tpw_li_flying_state(uint32_t* out) {
    out[0] = gFlying->mGroups->mCount > 0 ? static_cast<uint32_t>(gFlying->mGroups->mCount) : 0u;
    return 1;
}
}
