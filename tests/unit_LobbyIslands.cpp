// Test library for src/game/online/lobby_island.cpp (libtpw_unit_LobbyIslands.dylib; see
// tests/test_LobbyIslands.py): the lobby's island screen, its set-up, island list, show and hide,
// driven by scripts over two theme-island screens and up to eight islands.
//
// Real: CLBIsland's constructor, CLBScreenComponentIslands and CLBScreenComponentThemeIslands (the
// members of the unit), the island list. Fakes that record: the screen base class, the lobby's
// generator (CLBLobby::GetRand), view, mesh system, flying meshes, mesh instances, sound, the island
// lobby's panels, the online system, the rain, the theme descriptor parser and the signs; every
// island's destructor. The strings are functional.
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "FS_Online.h"
#include "Game.h"
#include "TPSound.h"
#include "TPString.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/msl.h"
#include "tb_rain.h"

#include "fake_strings.h"

using namespace calllog;

namespace {
enum { kScreens = 2, kIslands = 8, kMeshes = 64, kRows = 4 };
// The labels of the fixed objects. The screens are 1 and 2, the islands 10..17.
enum {
    kLabelScreen = 1, kLabelLobby = 3, kLabelGlobals = 4, kLabelView = 5, kLabelMeshes = 6, kLabelFlying = 7,
    kLabelRain = 8, kLabelOnline = 9, kLabelIsland = 10, kLabelGame = 18, kLabelSign = 70, kLabelPark = 80
};

alignas(16) unsigned char gLobbyStore[sizeof(CLBLobby)];
alignas(16) unsigned char gGlobalsStore[sizeof(CLBLobbyGlobals)];
alignas(16) unsigned char gViewStore[sizeof(CLBLobbyView)];
alignas(16) unsigned char gMeshesStore[64];
alignas(16) unsigned char gFlyingStore[64];
alignas(16) unsigned char gRainStore[64];
alignas(16) unsigned char gOnlineStore[sizeof(CFSOnlineSystem)];
alignas(16) unsigned char gMeshPool[kMeshes][16];
alignas(16) unsigned char gParks[2][16];
char16_t gSigns[2][8] = {u"AB", u"cde"};
int32_t gSoundRows[kRows * 2];
CLBScreenComponentThemeIslands* gScreens[kScreens];
bool gScreenAlive[kScreens];
CLBIsland* gIslands[kIslands];
bool gIslandAlive[kIslands];
// The blocks the screens and islands are made in, all taken before the case starts so that no
// object ever gets the address of a destroyed one (the PowerPC side's memory is not reused either,
// and a stale pointer must name the same object on both sides). Their state: 0 not used, 1 made,
// 2 freed by the code under test.
void* gScreenBlocks[kScreens];
int gScreenBlockState[kScreens];
void* gIslandBlocks[kIslands];
int gIslandBlockState[kIslands];
int32_t gMeshCount;

float FromBits(int32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
int64_t Hash(const char* text) {
    uint32_t h = 2166136261u;
    for (; text != nullptr && *text != 0; ++text) h = (h ^ static_cast<unsigned char>(*text)) * 16777619u;
    return h;
}
int64_t Hash16(const char16_t* text) {
    uint32_t h = 2166136261u;
    for (; text != nullptr && *text != 0; ++text) h = (h ^ static_cast<uint32_t>(*text & 0xff)) * 16777619u;
    return h;
}
// A wide text as ASCII in `buffer` (anything else as '?'): the fakes use no heap, since the state
// compares operator new's and delete's counts.
const char* Ascii(const char16_t* text, char* buffer) {
    std::size_t n = 0;
    for (; text != nullptr && text[n] != 0 && n < 159; ++n) buffer[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    if (text == nullptr) std::strcpy(buffer, "(null)");
    else buffer[n] = 0;
    return buffer;
}
// The slot of an island, -1 for none, -9 for a pointer that is none of them.
int64_t IslandSlot(const CLBIsland* island) {
    if (island == nullptr) return -1;
    for (int i = 0; i < kIslands; ++i)
        if (gIslands[i] == island) return i;
    return -9;
}
int64_t ScreenSlot(const void* screen) {
    if (screen == nullptr) return -1;
    for (int i = 0; i < kScreens; ++i)
        if (gScreens[i] == screen) return i;
    return -9;
}
}  // namespace

// ---- globals of the lobby and of other files
CLBLobby* gLobby;
CLBLobbyView* gLobbyView;
CLBLobbyMeshes* gLobbyMeshes;
CLBFlyingMeshSystem* gLobbyFlyingMeshes;
CFSOnlineSystem* gOnlineSystem;
CRain* gei_Rain;
int32_t* gLobbyIslandSounds;
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;

// ---- fakes
// The screen base class: no screen system here. It keeps the "visible" word at zero; IsVisible
// answers the word unless the case queued answers.
CLBScreenComponent::CLBScreenComponent() : mVisible(0) { ev("ScreenBase.ctor", {lab(this)}); }
CLBScreenComponent::~CLBScreenComponent() { ev("ScreenBase.dtor", {lab(this)}); }
void CLBScreenComponent::Show() { mVisible = 1; }
void CLBScreenComponent::Hide() { mVisible = 0; }
int32_t CLBScreenComponent::IsVisible() {
    ev("IsVisible", {lab(this)});
    return static_cast<int32_t>(ret("IsVisible", mVisible));
}

// The theme islands' other members (nothing here calls them; the destructor is the class's key function).
CLBScreenComponentThemeIslands::~CLBScreenComponentThemeIslands() {}
void CLBScreenComponentThemeIslands::Tick() {}
void CLBScreenComponentThemeIslands::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentThemeIslands::ProcessCancel() { return 0; }
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {}   // (the class's vtable has it; src/game/online/lobby_island.cpp)
// (The theme islands' own copy of SetCurrentIsland, src/game/online/lobby_island.cpp: the same text as the base class's.)
void CLBScreenComponentThemeIslands::SetCurrentIsland(const CLBIsland* island) { CLBScreenComponentIslands::SetCurrentIsland(island); }

// What the theme islands do after the parser (not translated): the current island is the one at list
// position `k` of the queue "ResetCurrent" (nothing when there is no such island), and the camera state 0.
void CLBScreenComponentThemeIslands::ResetCurrentIsland(int32_t a) {
    ev("ResetCurrentIsland", {lab(this), a});
    const int64_t k = ret("ResetCurrent", -1);
    int64_t n = 0;
    for (const auto* node = mIslands->mHead; node != nullptr; node = node->mNext, ++n) {
        if (n == k) {
            mCurrent = node->mData;
            mCameraState = 0;
            break;
        }
    }
}
void CLBScreenComponentThemeIslands::ResetAdvisorTimeout() { ev("ResetAdvisorTimeout", {lab(this)}); }
void CLBScreenComponentThemeIslands::UpdateSigns() { ev("UpdateSigns", {lab(this)}); }
void CLBScreenComponentIslands::ParseThemeDescriptor(const char* theme, const CFSOnlinePark* park, int32_t order,
                                                     const char16_t* gate1, const char16_t* gate2) {
    char text1[160], text2[160];
    ev("ParseThemeDescriptor", {lab(this), lab(park), order}, {theme, Ascii(gate1, text1), Ascii(gate2, text2)});
}

// An island's destructor: records, and marks the island dead (the real one leaves its screen's list,
// which the scripts do not need: only listed islands are ever destroyed).
CLBIsland::~CLBIsland() {
    const int32_t label = lab(this);
    ev("CLBIsland.dtor", {label});
    if (label >= kLabelIsland && label < kLabelIsland + kIslands) {
        gIslandAlive[label - kLabelIsland] = false;
        gIslandBlockState[label - kLabelIsland] = 2;   // (every destruction in the unit is a delete)
    }
}

// The lobby's generator and the C library's.
uint32_t CLBLobby::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("Rand", 0));
}
int32_t msl_rand() {
    ev("rand");
    return static_cast<int32_t>(ret("rand", 0));
}

// The view, the mesh system, the flying meshes.
void CLBLobbyView::SetGlobalAmbientLight(int32_t enabled, uint32_t color) { ev("SetGlobalAmbientLight", {lab(this), enabled, color}); }
void CLBLobbyView::SetGlobalDirectionalLight(int32_t enabled, const uint32_t* color, const s3DPoint* normal, int32_t followsView) {
    ev("SetGlobalDirectionalLight", {lab(this), enabled, color != nullptr ? static_cast<int64_t>(*color) : -1, lab(normal), followsView});
}
void CLBLobbyView::SetFogOn() { ev("SetFogOn", {lab(this)}); }
void CLBLobbyView::SetFogSettings(uint32_t color, float nearDistance, float farDistance) {
    ev("SetFogSettings", {lab(this), color, fbits(nearDistance), fbits(farDistance)});
}
void CLBLobbyMeshes::ShowHeightfield() { ev("ShowHeightfield", {lab(this)}); }
void CLBLobbyMeshes::HideHeightfield() { ev("HideHeightfield", {lab(this)}); }
void CLBLobbyMeshes::DeleteMeshInstanceListEntries() { ev("DeleteMeshInstanceListEntries", {lab(this)}); }
// Answers a new mesh instance (a blank block with the next free label).
CLBMeshInstance* CLBLobbyMeshes::InstanceMeshType(const char* folder, const char* name, int32_t overMapRange, uint32_t a,
                                                  const char16_t* sign1, const char16_t* sign2) {
    ev("InstanceMeshType", {lab(this), overMapRange, a, lab(sign1), lab(sign2)}, {folder, name});
    unsigned char* block = gMeshPool[gMeshCount++ % kMeshes];
    std::memset(block, 0xa5, 16);
    newlab(block);
    return reinterpret_cast<CLBMeshInstance*>(block);
}
void CLBMeshInstance::RotateY(float angle) { ev("RotateY", {lab(this), fbits(angle)}); }
void CLBMeshInstance::SetPosition(const s3DPoint& position) {
    ev("SetPosition", {lab(this), fbits(position.x), fbits(position.y), fbits(position.z)});
}
void CLBFlyingMeshSystem::DeleteFlyingMeshGroupListEntries() { ev("DeleteFlyingMeshGroupListEntries", {lab(this)}); }

// The island lobby's panels, the sounds, the rain, the online system.
void CreateIslandLobbyController() { ev("CreateIslandLobbyController"); }
void RemoveIslandLobbyController() { ev("RemoveIslandLobbyController"); }
void ShowIslandLobbyController() { ev("ShowIslandLobbyController"); }
void HideIslandLobbyController() { ev("HideIslandLobbyController"); }
void UpdateIslandLobbyUI() { ev("UpdateIslandLobbyUI"); }
int32_t SND_RegisterLocalLobbySounds(char* name) {
    ev("SND_RegisterLocalLobbySounds", {}, {name});
    return static_cast<int32_t>(ret("SND_RegisterLocalLobbySounds", 0));
}
void SND_FreeLocalLobbySounds() { ev("SND_FreeLocalLobbySounds"); }
void SOUND_FadeSoundEffect(int32_t sound) { ev("SOUND_FadeSoundEffect", {sound}); }
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("SOUND_SendEvent", 0));
}
void SOUND_Tick() { ev("SOUND_Tick"); }
int32_t CRain::GameLoad() {
    ev("CRain.GameLoad", {lab(this)});
    return 0;
}
int32_t CRain::Close() {
    ev("CRain.Close", {lab(this)});
    return 0;
}
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem.ctor", {newlab(this)}); }
void CFSOnlineSystem::StopProcessing() { ev("CFSOnlineSystem.StopProcessing", {lab(this)}); }

// The game's theme list: the unicode name is the folder name widened, the gate names are made from it.
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    ev("StringToUniString", {}, {in.mData});
    char16_t wide[200];
    std::size_t n = 0;
    for (const char* p = in.mData; *p != 0 && n < 199; ++p) wide[n++] = static_cast<unsigned char>(*p);
    wide[n] = 0;
    out = wide;
    return 1;
}
void CGame::GetLocalisedThemeGateNames(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& gate1,
                                       TbDynamicStringTemplate<char16_t>& gate2) const {
    char text[160];
    ev("GetLocalisedThemeGateNames", {lab(this)}, {Ascii(theme.mData, text)});
    char16_t name[200];
    std::size_t n = 0;
    for (; theme.mData[n] != 0 && n < 190; ++n) name[3 + n] = theme.mData[n];
    name[3 + n] = 0;
    name[0] = u'g';
    name[1] = u'1';
    name[2] = u':';
    gate1 = name;
    name[1] = u'2';
    gate2 = name;
}

// The game's themes: the names, one to a line (kept in nodes made here, never freed).
static void SetThemes(const char* themes) {
    using Node = TbTList<TbDynamicStringTemplate<char>*, TbDynamicStringTemplate<char>*>::CNode;
    gGame.mThemes.mHead = gGame.mThemes.mTail = nullptr;
    gGame.mThemes.mCount = 0;
    std::string all(themes);
    std::size_t start = 0;
    while (!all.empty() && start <= all.size()) {
        std::size_t end = all.find('\n', start);
        if (end == std::string::npos) end = all.size();
        Node* node = new Node(new TbDynamicStringTemplate<char>(all.substr(start, end - start).c_str()));
        node->mPrev = gGame.mThemes.mTail;
        if (gGame.mThemes.mTail != nullptr) gGame.mThemes.mTail->mNext = node;
        else gGame.mThemes.mHead = node;
        gGame.mThemes.mTail = node;
        ++gGame.mThemes.mCount;
        start = end + 1;
    }
}

extern "C" {
// The world. world: the view's position and target (six words), the view's and the description's
// field of view, 1 when the online system exists, the sound table's eight words. Labels: see above.
void tpw_li_begin(const uint32_t* world, const char* themes) {
    std::memset(gLobbyStore, 0xa5, sizeof gLobbyStore);
    std::memset(gGlobalsStore, 0xa5, sizeof gGlobalsStore);
    std::memset(gViewStore, 0xa5, sizeof gViewStore);
    std::memset(gOnlineStore, 0xa5, sizeof gOnlineStore);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyStore);
    gLobby->mGlobals = reinterpret_cast<CLBLobbyGlobals*>(gGlobalsStore);
    gLobby->mGlobals->mIslandFov = FromBits(world[7]);
    gLobbyView = reinterpret_cast<CLBLobbyView*>(gViewStore);
    gLobbyView->mPosition = {FromBits(world[0]), FromBits(world[1]), FromBits(world[2])};
    gLobbyView->mTarget = {FromBits(world[3]), FromBits(world[4]), FromBits(world[5])};
    gLobbyView->mFov = FromBits(world[6]);
    gLobbyMeshes = reinterpret_cast<CLBLobbyMeshes*>(gMeshesStore);
    gLobbyFlyingMeshes = reinterpret_cast<CLBFlyingMeshSystem*>(gFlyingStore);
    gei_Rain = reinterpret_cast<CRain*>(gRainStore);
    gOnlineSystem = world[8] != 0 ? reinterpret_cast<CFSOnlineSystem*>(gOnlineStore) : nullptr;
    for (int i = 0; i < kRows * 2; ++i) gSoundRows[i] = static_cast<int32_t>(world[9 + i]);
    gLobbyIslandSounds = gSoundRows;
    SetThemes(themes);
    for (int i = 0; i < kScreens; ++i) {
        if (gScreenBlocks[i] != nullptr && gScreenBlockState[i] != 2) ::operator delete(gScreenBlocks[i]);
        gScreenBlocks[i] = ::operator new(sizeof(CLBScreenComponentThemeIslands));
        gScreenBlockState[i] = 0;
    }
    for (int i = 0; i < kIslands; ++i) {
        if (gIslandBlocks[i] != nullptr && gIslandBlockState[i] != 2) ::operator delete(gIslandBlocks[i]);
        gIslandBlocks[i] = ::operator new(sizeof(CLBIsland));
        gIslandBlockState[i] = 0;
    }
    for (auto& screen : gScreens) screen = nullptr;
    for (auto& alive : gScreenAlive) alive = false;
    for (auto& island : gIslands) island = nullptr;
    for (auto& alive : gIslandAlive) alive = false;
    gMeshCount = 0;
    tpw_cl_label(kLabelLobby, gLobbyStore);
    tpw_cl_label(kLabelGlobals, gGlobalsStore);
    tpw_cl_label(kLabelView, gViewStore);
    tpw_cl_label(kLabelMeshes, gMeshesStore);
    tpw_cl_label(kLabelFlying, gFlyingStore);
    tpw_cl_label(kLabelRain, gRainStore);
    tpw_cl_label(kLabelOnline, gOnlineStore);
    tpw_cl_label(kLabelGame, &gGame);
    tpw_cl_label(kLabelSign, gSigns[0]);
    tpw_cl_label(kLabelSign + 1, gSigns[1]);
    tpw_cl_label(kLabelPark, gParks[0]);
    tpw_cl_label(kLabelPark + 1, gParks[1]);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}

// The ops (a: twelve words). texts: name, folder, mesh A, mesh B and title of an island, one to a line.
int64_t tpw_li_op(int32_t op, const int32_t* a, const char* texts) {
    CLBScreenComponentThemeIslands* screen = gScreens[a[0] & 1];
    switch (op) {
    case 1: {   // a screen as CLBScreenSystem::Init makes it: a block of 0xa5, the base constructor, the derived vtable
        void* block = gScreenBlocks[a[0] & 1];
        gScreenBlockState[a[0] & 1] = 1;
        ++tpw_cl_heap[0];   // (the game's operator new)
        std::memset(block, 0xa5, sizeof(CLBScreenComponentThemeIslands));
        tpw_cl_label(kLabelScreen + (a[0] & 1), block);
        gScreens[a[0] & 1] = new (block) CLBScreenComponentThemeIslands;
        gScreenAlive[a[0] & 1] = true;
        return 0;
    }
    case 2:   // the base destructor (the derived one here is empty); with a flag above 0 the block is freed
        if (a[1] > 0) {
            delete screen;
            gScreenBlockState[a[0] & 1] = 2;
        } else {
            screen->~CLBScreenComponentThemeIslands();
        }
        gScreenAlive[a[0] & 1] = false;
        return 0;
    case 3: {   // an island: a[0] its slot, a[1] its screen, a[2] order, a[3] angle, a[4..6] position, a[7..8] signs, a[9] park
        // (The texts are split in place in a local buffer: nothing here may touch operator new, whose
        // counts the state compares.)
        char buffer[512];
        std::strncpy(buffer, texts, sizeof buffer - 1);
        buffer[sizeof buffer - 1] = 0;
        const char* part[5];
        char* cursor = buffer;
        for (auto& p : part) {
            p = cursor;
            while (*cursor != 0 && *cursor != '\n') ++cursor;
            if (*cursor == '\n') *cursor++ = 0;
        }
        const int32_t slot = a[0] & 7;
        char16_t title[128];
        std::size_t titleLength = 0;
        for (const char* t = part[4]; *t != 0 && titleLength < 127; ++t) title[titleLength++] = static_cast<unsigned char>(*t);
        title[titleLength] = 0;
        void* block = gIslandBlocks[slot];
        gIslandBlockState[slot] = 1;
        ++tpw_cl_heap[0];   // (the game's operator new)
        std::memset(block, 0xa5, sizeof(CLBIsland));
        tpw_cl_label(kLabelIsland + slot, block);
        const s3DPoint position = {FromBits(a[4]), FromBits(a[5]), FromBits(a[6])};
        const char16_t* sign1 = a[7] != 0 ? gSigns[(a[7] - 1) & 1] : nullptr;
        const char16_t* sign2 = a[8] != 0 ? gSigns[(a[8] - 1) & 1] : nullptr;
        const CFSOnlinePark* park = a[9] != 0 ? reinterpret_cast<const CFSOnlinePark*>(gParks[(a[9] - 1) & 1]) : nullptr;
        gIslands[slot] = new (block) CLBIsland(gScreens[a[1] & 1], a[2], part[0], part[1], part[2], part[3], FromBits(a[3]),
                                               position, title, park, sign1, sign2);
        gIslandAlive[slot] = true;
        return 0;
    }
    case 4: screen->Register(gIslands[a[1] & 7]); return 0;
    case 5: screen->UnRegister(gIslands[a[1] & 7]); return 0;
    case 6: screen->SetCurrentIsland(a[1] < 0 ? nullptr : gIslands[a[1] & 7]); return 0;
    case 7: static_cast<CLBScreenComponent*>(screen)->Init(); return 0;
    case 8: static_cast<CLBScreenComponent*>(screen)->Render(); return 0;
    case 9: static_cast<CLBScreenComponent*>(screen)->Close(); return 0;
    case 10: static_cast<CLBScreenComponent*>(screen)->Show(); return 0;
    case 11: static_cast<CLBScreenComponent*>(screen)->Hide(); return 0;
    case 12: screen->CLBScreenComponentIslands::Show(); return 0;
    case 13: screen->CLBScreenComponentIslands::Hide(); return 0;
    case 14: screen->DeleteIslandListEntries(); return 0;
    case 15: screen->ShowPanels(); return 0;
    case 16: screen->HidePanels(); return 0;
    case 17: screen->mVisible = a[1]; return 0;
    case 18: screen->mCurrent = a[1] < 0 ? nullptr : gIslands[a[1] & 7]; return 0;
    case 19:
        gIslands[a[0] & 7]->mSound1 = a[1];
        gIslands[a[0] & 7]->mSound2 = a[2];
        return 0;
    case 20: gOnlineSystem = a[0] != 0 ? reinterpret_cast<CFSOnlineSystem*>(gOnlineStore) : nullptr; return 0;
    case 21: screen->Register(nullptr); return 0;
    }
    return -99;
}

// The state: the view's words, the online system, then each live screen (its words in the order of
// the Mac offsets 0x14..0xe4) and each live island, then operator new's and delete's counts.
int32_t tpw_li_state(int64_t* out) {
    int64_t* p = out;
    const CLBLobbyView* view = gLobbyView;
    *p++ = fbits(view->mPosition.x);
    *p++ = fbits(view->mPosition.y);
    *p++ = fbits(view->mPosition.z);
    *p++ = fbits(view->mTarget.x);
    *p++ = fbits(view->mTarget.y);
    *p++ = fbits(view->mTarget.z);
    *p++ = fbits(view->mFov);
    *p++ = fbits(view->mLightUpOffset);
    *p++ = fbits(view->mLightRightOffset);
    *p++ = view->mLightDownward;
    *p++ = lab(gOnlineSystem);
    for (int i = 0; i < kScreens; ++i) {
        if (!gScreenAlive[i]) continue;
        const CLBScreenComponentThemeIslands* s = gScreens[i];
        *p++ = i;
        *p++ = s->mVisible;
        *p++ = s->mIslands->mCount;
        int32_t n = 0;
        for (const auto* node = s->mIslands->mHead; node != nullptr && n < 40; node = node->mNext, ++n) *p++ = IslandSlot(node->mData);
        *p++ = IslandSlot(s->mCurrent);
        *p++ = IslandSlot(s->mLast);
        *p++ = s->mCameraState;
        *p++ = fbits(s->mAngle);
        *p++ = fbits(s->mSpinSpeed);
        *p++ = fbits(s->mRadius);
        *p++ = fbits(s->mHeight);
        for (const s3DPoint* point : {&s->mCameraPos, &s->mCameraGoal, &s->mLookAt, &s->mLookAtGoal, &s->mRoamCentre, &s->mRoamExtent}) {
            *p++ = fbits(point->x);
            *p++ = fbits(point->y);
            *p++ = fbits(point->z);
        }
        *p++ = fbits(s->mRoamSpeed);
        *p++ = fbits(s->mRoamArriveDistSq);
        for (const s3DPoint* point : {&s->mRoamPos, &s->mRoamGoal, &s->mRoamDir, &s->mRoamGoalDir}) {
            *p++ = fbits(point->x);
            *p++ = fbits(point->y);
            *p++ = fbits(point->z);
        }
        *p++ = fbits(s->mLookMaxSpeed);
        *p++ = fbits(s->mLookSpeed);
        *p++ = fbits(s->mLookSlowDistSq);
        for (const s3DPoint* point : {&s->mLookPos, &s->mLookGoal, &s->mLookDir, &s->mLookGoalDir}) {
            *p++ = fbits(point->x);
            *p++ = fbits(point->y);
            *p++ = fbits(point->z);
        }
        *p++ = s->mFE4;
    }
    for (int i = 0; i < kIslands; ++i) {
        if (gIslands[i] == nullptr || !gIslandAlive[i]) continue;
        const CLBIsland* island = gIslands[i];
        *p++ = i;
        *p++ = lab(island->mMeshA);
        *p++ = lab(island->mMeshB);
        *p++ = ScreenSlot(island->mOwner);
        *p++ = island->mShown;
        *p++ = fbits(island->mAngle);
        *p++ = island->mSoundSet;
        *p++ = island->mSound1;
        *p++ = island->mSound2;
        *p++ = island->mOrder;
        *p++ = Hash(island->mName.mData);
        *p++ = fbits(island->mPosition.x);
        *p++ = fbits(island->mPosition.y);
        *p++ = fbits(island->mPosition.z);
        *p++ = Hash16(island->mTitle.mData);
        *p++ = island->mLightning;
        *p++ = island->mRain;
        *p++ = island->mSky[0];
        *p++ = island->mSky[1];
        *p++ = island->mSky[2];
        *p++ = lab(island->mPark);
    }
    *p++ = tpw_cl_heap[0];
    *p++ = tpw_cl_heap[2];
    return static_cast<int32_t>(p - out);
}
}
