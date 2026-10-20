// Test library for src/lobby_islands_rest.cpp (libtpw_unit_LobbyIslandsRest.dylib; see
// tests/test_LobbyIslandsRest.py): the lobby's theme descriptor parser and the rest of the island and
// theme island members, driven by scripts over two theme-island screens.
//
// Real: the unit (src/lobby_islands_rest.cpp), the island screen's set-up, list, show and hide
// (src/lobby_islands.cpp, with CLBIsland's constructor), and the lobby description's string reader and its
// folder (src/lobby_parser.cpp). Fakes that record: the screen base class, the view, the mesh system and its
// instances, the flying meshes and their groups, the sounds, the island lobby's panels, the online system,
// the rain, the game (its theme list, the current theme, the gate names), the master save and the user info,
// the game type, the file system and the heap of the files, and the C library's rand. The strings are
// functional. The theme islands' Tick belongs to src/lobby_islands_tick.cpp, which is not linked here.
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "LbFile.h"
#include "LbTime.h"
#include "TPSound.h"
#include "TPString.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "tb_rain.h"

#include "fake_strings.h"

using namespace calllog;

namespace {
enum { kScreens = 2, kMeshes = 256, kRows = 4, kParks = 3 };
// The labels of the fixed objects. The screens are 1 and 2; new objects get labels from 100.
enum {
    kLabelScreen = 1, kLabelLobby = 3, kLabelGlobals = 4, kLabelView = 5, kLabelMeshes = 6, kLabelFlying = 7,
    kLabelRain = 8, kLabelOnline = 9, kLabelGame = 18, kLabelFile = 20, kLabelUser = 21, kLabelMaster = 22,
    kLabelSign = 70, kLabelOnlinePark = 80, kLabelParkData = 90
};
// The words of a world (the Python module's WORLD).
enum {
    kViewPosition = 0, kViewFov = 6, kDescriptionFov = 7, kOnlineExists = 8, kSoundTable = 9, kCameraX = 17, kCameraY = 27,
    kSeed = 37, kLobbyState = 38, kGameType = 39, kUserId = 40, kUserExists = 41, kMasterExists = 42, kWorldWords = 44
};

alignas(16) unsigned char gLobbyStore[sizeof(CLBLobby)];
alignas(16) unsigned char gGlobalsStore[sizeof(CLBLobbyGlobals)];
alignas(16) unsigned char gViewStore[sizeof(CLBLobbyView)];
alignas(16) unsigned char gMeshesStore[64];
alignas(16) unsigned char gFlyingStore[64];
alignas(16) unsigned char gRainStore[64];
alignas(16) unsigned char gOnlineStore[sizeof(CFSOnlineSystem)];
alignas(16) unsigned char gMeshPool[kMeshes][16];
alignas(16) unsigned char gOnlineParks[2][16];
alignas(16) unsigned char gParkData[kParks][16];
alignas(16) unsigned char gUserStore[sizeof(CFSUserInfo)];
alignas(16) unsigned char gMasterStore[sizeof(CFSGlobalMasterSave)];
char gFileObject[8];
char16_t gSigns[2][8] = {u"AB", u"cde"};
int32_t gSoundRows[kRows * 2];
CLBScreenComponentThemeIslands* gScreens[kScreens];
bool gScreenAlive[kScreens];
// The blocks the screens are made in, all taken before the case starts so that no screen ever gets the
// address of a destroyed one (the PowerPC side's memory is not reused and labels go by address). Their
// state: 0 not used, 1 made, 2 freed by the code under test.
void* gScreenBlocks[kScreens];
int gScreenBlockState[kScreens];
int32_t gMeshCount;
int32_t gWorldGameType;
int32_t gWorldUser;
std::string gMissingTheme;                              // the master save knows no park of this name
std::vector<std::pair<std::string, std::string>> gFiles;   // theme name, file text
const char* gFileText;
uint32_t gFileSize;

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
// The whole of each 16-bit character (a title's characters are widened with their sign).
int64_t Hash16(const char16_t* text) {
    uint32_t h = 2166136261u;
    for (; text != nullptr && *text != 0; ++text) h = (h ^ static_cast<uint32_t>(*text)) * 16777619u;
    return h;
}
// A wide text as ASCII in `buffer` (anything else as '?').
const char* Ascii(const char16_t* text, char* buffer) {
    std::size_t n = 0;
    for (; text != nullptr && text[n] != 0 && n < 159; ++n) buffer[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    if (text == nullptr) std::strcpy(buffer, "(null)");
    else buffer[n] = 0;
    return buffer;
}
int64_t ScreenSlot(const void* screen) {
    if (screen == nullptr) return -1;
    for (int i = 0; i < kScreens; ++i)
        if (gScreens[i] == screen) return i;
    return -9;
}
// The position of an island in its screen's list (-1 for none, -9 for an island that is not listed).
int64_t ListIndex(const CLBScreenComponentIslands* screen, const CLBIsland* island) {
    if (island == nullptr) return -1;
    int64_t n = 0;
    for (const auto* node = screen->mIslands->mHead; node != nullptr; node = node->mNext, ++n)
        if (node->mData == island) return n;
    return -9;
}
// Whether the list holds a NULL entry (the scripts then skip what would read through it).
bool HasNullEntry(const CLBScreenComponentIslands* screen) {
    for (const auto* node = screen->mIslands->mHead; node != nullptr; node = node->mNext)
        if (node->mData == nullptr) return true;
    return false;
}
// Whether UpdateSigns (or, with `themes`, a Show, which makes the islands of the game's themes first) would
// ask the master save for a theme it misses while a player is logged in (the original then calls through
// NULL). The user info is made by the first call if it is not there: the world's user then.
bool SignsWouldFail(const CLBScreenComponentIslands* screen, bool themes) {
    const int32_t user = gUserInfo != nullptr ? gUserInfo->mGameUser : gWorldUser;
    if (user == -1) return false;
    for (const auto* node = screen->mIslands->mHead; node != nullptr; node = node->mNext)
        if (gMissingTheme == node->mData->mName.mData) return true;
    if (themes) {
        for (const auto* node = gGame.mThemes.mHead; node != nullptr; node = node->mNext)
            if (gMissingTheme == node->mData->mData) return true;
    }
    return false;
}
// The k-th island of the screen's list (k taken modulo the count); NULL for an empty list.
CLBIsland* IslandAt(const CLBScreenComponentIslands* screen, int32_t k) {
    const int32_t count = screen->mIslands->mCount;
    if (count <= 0) return nullptr;
    int32_t n = static_cast<int32_t>(static_cast<uint32_t>(k) % static_cast<uint32_t>(count));
    const auto* node = screen->mIslands->mHead;
    for (; n > 0 && node != nullptr; --n) node = node->mNext;
    return node != nullptr ? node->mData : nullptr;
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
CFSUserInfo* gUserInfo;
CFSGlobalMasterSave* gMasterSave;
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;

// ---- fakes: the screen base class (no screen system here). It keeps the "visible" word at zero; IsVisible
// answers the word unless the case queued answers.
CLBScreenComponent::CLBScreenComponent() : mVisible(0) { ev("ScreenBase.ctor", {lab(this)}); }
CLBScreenComponent::~CLBScreenComponent() { ev("ScreenBase.dtor", {lab(this)}); }
void CLBScreenComponent::Show() { mVisible = 1; }
void CLBScreenComponent::Hide() { mVisible = 0; }
int32_t CLBScreenComponent::IsVisible() {
    ev("IsVisible", {lab(this)});
    return static_cast<int32_t>(ret("IsVisible", mVisible));
}

// The theme islands' Tick (src/lobby_islands_tick.cpp, not linked here; nothing calls it).
void CLBScreenComponentThemeIslands::Tick() {}
// The base class's moves, called by the theme islands' own: recorded only.
void CLBScreenComponentIslands::GotoNextIsland() { ev("Islands.GotoNextIsland", {lab(this)}); }
void CLBScreenComponentIslands::GotoPreviousIsland() { ev("Islands.GotoPreviousIsland", {lab(this)}); }

// The lobby's generator is real (the unit); the C library's is not.
int32_t msl_rand() {
    ev("rand");
    return static_cast<int32_t>(ret("rand", 0));
}

// ---- fakes: the view, the mesh system, the flying meshes and their groups
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
void CLBMeshInstance::Show() { ev("CLBMeshInstance.Show", {lab(this)}); }
void CLBMeshInstance::PlayAnim(int32_t anim, int32_t loop) { ev("CLBMeshInstance.PlayAnim", {lab(this), anim, loop}); }
void CLBMeshInstance::SetRideSigns(const char16_t* sign1, const char16_t* sign2) {
    char text1[160], text2[160];
    ev("CLBMeshInstance.SetRideSigns", {lab(this)}, {Ascii(sign1, text1), Ascii(sign2, text2)});
}
void CLBFlyingMeshSystem::DeleteFlyingMeshGroupListEntries() { ev("DeleteFlyingMeshGroupListEntries", {lab(this)}); }
// A group of flying meshes: made by the parser and never freed (the real one registers with the system).
CLBFlyingMeshGroup::CLBFlyingMeshGroup(int32_t number, const char* folder, const char* name, const s3DPoint& position,
                                       const s3DPoint& direction, float a, float b) {
    ev("CLBFlyingMeshGroup.ctor",
       {newlab(this), number, fbits(position.x), fbits(position.y), fbits(position.z), fbits(direction.x), fbits(direction.y),
        fbits(direction.z), fbits(a), fbits(b)},
       {folder, name});
}
void CLBFlyingMeshGroup::Show() { ev("CLBFlyingMeshGroup.Show", {lab(this)}); }
// (The group has a vtable since the flying meshes were translated, and the fake constructor above sets it: its
// virtual members need a definition, which nothing calls.)
CLBFlyingMeshGroup::~CLBFlyingMeshGroup() {}
void CLBFlyingMeshGroup::Register(CLBFlyingMeshInstance*) {}
void CLBFlyingMeshGroup::UnRegister(CLBFlyingMeshInstance*) {}

// ---- fakes: the island lobby's panels, the sounds, the rain, the online system
void CreateIslandLobbyController() { ev("CreateIslandLobbyController"); }
void RemoveIslandLobbyController() { ev("RemoveIslandLobbyController"); }
void ShowIslandLobbyController() { ev("ShowIslandLobbyController"); }
void HideIslandLobbyController() { ev("HideIslandLobbyController"); }
void UpdateIslandLobbyUI() { ev("UpdateIslandLobbyUI"); }
void KeyParticlePOUFFE() { ev("KeyParticlePOUFFE"); }
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

// ---- fakes: the game (its themes and the current theme), the gate names
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
void CGame::SetCurrentTheme(const TbDynamicStringTemplate<char>& name) { ev("CGame.SetCurrentTheme", {lab(this)}, {name.mData}); }

// ---- fakes: the master save, the user info, the game type, the clock, the case-blind compare
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave.ctor", {newlab(this)}); }
// The record of a theme: one of three blocks by the name's hash, none for the one name the world misses.
CFSGlobalParkData* CFSGlobalMasterSave::GetInfoOn(const TbDynamicStringTemplate<char>& name) {
    ev("CFSGlobalMasterSave.GetInfoOn", {lab(this)}, {name.mData});
    if (std::strcmp(name.mData, "ClearMaster") == 0) gMasterSave = nullptr;   // the test's hook: the master save is gone on return
    if (gMissingTheme == name.mData) return nullptr;
    return reinterpret_cast<CFSGlobalParkData*>(gParkData[Hash(name.mData) % kParks]);
}
int32_t CFSGlobalMasterSave::HowManyKeys() const {
    ev("CFSGlobalMasterSave.HowManyKeys", {lab(this)});
    return static_cast<int32_t>(ret("Keys", 0));
}
int32_t CFSGlobalParkData::KeyCostToEnter() {
    ev("CFSGlobalParkData.KeyCostToEnter", {lab(this)});
    return static_cast<int32_t>(ret("Cost", lab(this), 0));
}
// A sign text: "SA" or "SB" and the park record's label.
static void WriteSign(char16_t* buffer, uint32_t size, char letter, int32_t label) {
    char text[16];
    std::snprintf(text, sizeof text, "S%c%d", letter, label);
    std::size_t n = 0;
    for (; text[n] != 0 && n + 1 < size; ++n) buffer[n] = static_cast<unsigned char>(text[n]);
    buffer[n] = 0;
}
void CFSGlobalParkData::GetSignNameA(char16_t* buffer, uint32_t size) {
    ev("CFSGlobalParkData.GetSignNameA", {lab(this), size});
    WriteSign(buffer, size, 'A', lab(this));
}
void CFSGlobalParkData::GetSignNameB(char16_t* buffer, uint32_t size) {
    ev("CFSGlobalParkData.GetSignNameB", {lab(this), size});
    WriteSign(buffer, size, 'B', lab(this));
}
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
CFSUserInfo::CFSUserInfo() {
    ev("CFSUserInfo.ctor", {newlab(this)});
    mGameUser = gWorldUser;
}
// The game type object is made on first use, from the options; here it takes the world's value, unrecorded.
CGameType::CGameType() : mType(static_cast<GameType>(gWorldGameType)) {}
uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock");
    return static_cast<uint32_t>(ret("Clock", 0));
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    for (;; ++a, ++b) {
        const int x = (*a >= 'A' && *a <= 'Z') ? *a + 32 : static_cast<unsigned char>(*a);
        const int y = (*b >= 'A' && *b <= 'Z') ? *b + 32 : static_cast<unsigned char>(*b);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
}

// ---- fakes: the files (one text per theme) and the heap they are read into
bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    gFileText = "";
    gFileSize = 0;
    // (No std::string here: the state compares operator new's counts.)
    const std::size_t length = std::strlen(name);
    for (const auto& file : gFiles) {
        const std::size_t theme = file.first.size();
        if (length == 11 + theme + 4 && std::strncmp(name, "data:lobby:", 11) == 0 &&
            std::strncmp(name + 11, file.first.c_str(), theme) == 0 && std::strcmp(name + 11 + theme, ".txt") == 0) {
            gFileText = file.second.c_str();
            gFileSize = static_cast<uint32_t>(file.second.size());
            break;
        }
    }
    return reinterpret_cast<bfl::FileSystem::CFile*>(gFileObject);
}
uint32_t TPFile_Seek(bfl::FileSystem::CFile* file, uint32_t offset, uint32_t whence) {
    ev("TPFile_Seek", {lab(file), offset, whence});
    return 0;
}
int32_t TPFile_Size(bfl::FileSystem::CFile* file) {
    ev("TPFile_Size", {lab(file)});
    return static_cast<int32_t>(gFileSize);
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {lab(dest), size, count, lab(file)});
    std::memcpy(dest, gFileText, gFileSize);
    return count;
}
uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return 0;
}
// (One byte more than asked, a zero: a file without a line feed at its end finishes at that zero.)
void* LbMem_Alloc(uint32_t size) {
    void* block = std::calloc(1, static_cast<size_t>(size) + 1);
    ev("LbMem_Alloc", {size, newlab(block)});
    return block;
}
void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    forget(block);
    std::free(block);
}

// The string buffers of the string classes: the text itself, and its length by strlen.
template<> char* TbStringBase<char>::LockBuffer(uint32_t) { return mData; }
template<> void TbStringBase<char>::ReleaseBuffer(uint32_t) { mLength = static_cast<int32_t>(std::strlen(mData)); }

namespace {
// The game's themes: the names, one to a line (kept in nodes made here, never freed).
void SetThemes(const std::string& all) {
    using Node = TbTList<TbDynamicStringTemplate<char>*, TbDynamicStringTemplate<char>*>::CNode;
    gGame.mThemes.mHead = gGame.mThemes.mTail = nullptr;
    gGame.mThemes.mCount = 0;
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
// Splits `text` at `separator`.
std::vector<std::string> Split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = text.find(separator, start);
        if (end == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}
}  // namespace

extern "C" {
// The world. words: see the enum above; blob: the game's themes (one to a line), \x01, the files
// ("theme\x03text" joined by \x02), \x01, the theme the master save misses, \x01, the theme the player last
// entered. Labels: see above.
void tpw_lr_begin(const uint32_t* words, const char* blob) {
    const std::vector<std::string> parts = Split(blob, '\x01');
    std::memset(gLobbyStore, 0xa5, sizeof gLobbyStore);
    std::memset(gGlobalsStore, 0xa5, sizeof gGlobalsStore);
    std::memset(gViewStore, 0xa5, sizeof gViewStore);
    std::memset(gOnlineStore, 0xa5, sizeof gOnlineStore);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyStore);
    gLobby->mGlobals = reinterpret_cast<CLBLobbyGlobals*>(gGlobalsStore);
    gLobby->mGlobals->mIslandFov = FromBits(words[kDescriptionFov]);
    for (int i = 0; i < 10; ++i) {
        gLobby->mGlobals->mCameraX[i] = FromBits(words[kCameraX + i]);
        gLobby->mGlobals->mCameraY[i] = FromBits(words[kCameraY + i]);
    }
    gLobby->mStarted = words[kSeed];
    gLobby->mState = static_cast<int32_t>(words[kLobbyState]);
    gLobbyView = reinterpret_cast<CLBLobbyView*>(gViewStore);
    gLobbyView->mPosition = {FromBits(words[0]), FromBits(words[1]), FromBits(words[2])};
    gLobbyView->mTarget = {FromBits(words[3]), FromBits(words[4]), FromBits(words[5])};
    gLobbyView->mFov = FromBits(words[kViewFov]);
    gLobbyMeshes = reinterpret_cast<CLBLobbyMeshes*>(gMeshesStore);
    gLobbyFlyingMeshes = reinterpret_cast<CLBFlyingMeshSystem*>(gFlyingStore);
    gei_Rain = reinterpret_cast<CRain*>(gRainStore);
    gOnlineSystem = words[kOnlineExists] != 0 ? reinterpret_cast<CFSOnlineSystem*>(gOnlineStore) : nullptr;
    for (int i = 0; i < kRows * 2; ++i) gSoundRows[i] = static_cast<int32_t>(words[kSoundTable + i]);
    gLobbyIslandSounds = gSoundRows;
    gWorldGameType = static_cast<int32_t>(words[kGameType]);
    gWorldUser = static_cast<int32_t>(words[kUserId]);
    TheGameType().mType = static_cast<GameType>(gWorldGameType);   // (made now, unrecorded, if it was not)
    std::memset(gUserStore, 0xa5, sizeof gUserStore);
    gUserInfo = words[kUserExists] != 0 ? reinterpret_cast<CFSUserInfo*>(gUserStore) : nullptr;
    if (gUserInfo != nullptr) gUserInfo->mGameUser = gWorldUser;
    std::memset(gMasterStore, 0xa5, sizeof gMasterStore);
    gMasterSave = words[kMasterExists] != 0 ? reinterpret_cast<CFSGlobalMasterSave*>(gMasterStore) : nullptr;
    SetThemes(parts.size() > 0 ? parts[0] : "");
    gFiles.clear();
    if (parts.size() > 1 && !parts[1].empty()) {
        for (const std::string& entry : Split(parts[1], '\x02')) {
            const std::size_t at = entry.find('\x03');
            gFiles.emplace_back(entry.substr(0, at), at == std::string::npos ? "" : entry.substr(at + 1));
        }
    }
    gMissingTheme = parts.size() > 2 ? parts[2] : "";
    gLobbyIslandTheme = parts.size() > 3 ? parts[3].c_str() : "";
    for (int i = 0; i < kScreens; ++i) {
        if (gScreenBlocks[i] != nullptr && gScreenBlockState[i] != 2) ::operator delete(gScreenBlocks[i]);
        gScreenBlocks[i] = ::operator new(sizeof(CLBScreenComponentThemeIslands));
        gScreenBlockState[i] = 0;
    }
    for (auto& screen : gScreens) screen = nullptr;
    for (auto& alive : gScreenAlive) alive = false;
    gMeshCount = 0;
    gFileText = "";
    gFileSize = 0;
    tpw_cl_label(kLabelLobby, gLobbyStore);
    tpw_cl_label(kLabelGlobals, gGlobalsStore);
    tpw_cl_label(kLabelView, gViewStore);
    tpw_cl_label(kLabelMeshes, gMeshesStore);
    tpw_cl_label(kLabelFlying, gFlyingStore);
    tpw_cl_label(kLabelRain, gRainStore);
    tpw_cl_label(kLabelOnline, gOnlineStore);
    tpw_cl_label(kLabelGame, &gGame);
    tpw_cl_label(kLabelFile, gFileObject);
    tpw_cl_label(kLabelSign, gSigns[0]);
    tpw_cl_label(kLabelSign + 1, gSigns[1]);
    tpw_cl_label(kLabelOnlinePark, gOnlineParks[0]);
    tpw_cl_label(kLabelOnlinePark + 1, gOnlineParks[1]);
    for (int i = 0; i < kParks; ++i) tpw_cl_label(kLabelParkData + i, gParkData[i]);
    if (gUserInfo != nullptr) tpw_cl_label(kLabelUser, gUserInfo);
    if (gMasterSave != nullptr) tpw_cl_label(kLabelMaster, gMasterSave);
}

// The ops (a: twelve words). texts: see the Python module's list of ops.
int64_t tpw_lr_op(int32_t op, const int32_t* a, const char* texts) {
    CLBScreenComponentThemeIslands* screen = gScreens[a[0] & 1];
    const bool alive = gScreenAlive[a[0] & 1];
    if (op == 1) {   // a screen as CLBScreenSystem::Init makes it: a block of 0xa5, the base constructor, the derived vtable
        void* block = gScreenBlocks[a[0] & 1];
        gScreenBlockState[a[0] & 1] = 1;
        std::memset(block, 0xa5, sizeof(CLBScreenComponentThemeIslands));
        tpw_cl_label(kLabelScreen + (a[0] & 1), block);
        gScreens[a[0] & 1] = new (block) CLBScreenComponentThemeIslands;
        gScreenAlive[a[0] & 1] = true;
        return 0;
    }
    if (op == 25) {   // the game type
        TheGameType().mType = static_cast<GameType>(a[1]);
        return 0;
    }
    if (op == 26) {   // the user info: present with a user, or absent
        gWorldUser = a[1];
        if (a[0] != 0) {
            if (gUserInfo == nullptr) {
                std::memset(gUserStore, 0xa5, sizeof gUserStore);
                gUserInfo = reinterpret_cast<CFSUserInfo*>(gUserStore);
                tpw_cl_label(kLabelUser, gUserInfo);
            }
            gUserInfo->mGameUser = a[1];
        } else {
            gUserInfo = nullptr;
        }
        return 0;
    }
    if (op == 27) {   // the master save: present or absent
        if (a[0] != 0) {
            std::memset(gMasterStore, 0xa5, sizeof gMasterStore);
            gMasterSave = reinterpret_cast<CFSGlobalMasterSave*>(gMasterStore);
            tpw_cl_label(kLabelMaster, gMasterSave);
        } else {
            gMasterSave = nullptr;
        }
        return 0;
    }
    if (op == 28) {   // the master save "misses" a theme: GetInfoOn answers NULL for its name
        gMissingTheme = texts;
        return 0;
    }
    if (op == 29) {   // the theme the player last entered
        gLobbyIslandTheme = texts;
        return 0;
    }
    if (op == 18) {   // the lobby's generator
        return gLobby->GetRand();
    }
    if (!alive) return -5;
    switch (op) {
    case 2:   // the destructor; with a flag above 0 the block is freed. The islands the screen held are gone.
        if (a[1] > 0) {
            delete screen;
            gScreenBlockState[a[0] & 1] = 2;
        } else {
            screen->~CLBScreenComponentThemeIslands();
        }
        gScreenAlive[a[0] & 1] = false;
        return 0;
    case 3: {   // an island: a[1] order, a[2] angle, a[3..5] position, a[6..7] signs, a[8] park
        if (HasNullEntry(screen)) return -5;
        // (The texts are split in place in a local buffer.)
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
        char16_t title[128];
        std::size_t titleLength = 0;
        for (const char* t = part[4]; *t != 0 && titleLength < 127; ++t) title[titleLength++] = static_cast<unsigned char>(*t);
        title[titleLength] = 0;
        void* block = ::operator new(sizeof(CLBIsland));
        std::memset(block, 0xa5, sizeof(CLBIsland));
        const s3DPoint position = {FromBits(a[3]), FromBits(a[4]), FromBits(a[5])};
        const char16_t* sign1 = a[6] != 0 ? gSigns[(a[6] - 1) & 1] : nullptr;
        const char16_t* sign2 = a[7] != 0 ? gSigns[(a[7] - 1) & 1] : nullptr;
        const CFSOnlinePark* park = a[8] != 0 ? reinterpret_cast<const CFSOnlinePark*>(gOnlineParks[(a[8] - 1) & 1]) : nullptr;
        new (block) CLBIsland(screen, a[1], part[0], part[1], part[2], part[3], FromBits(a[2]), position, title, park, sign1, sign2);
        return 0;
    }
    case 4:   // Show of the k-th island
        if (CLBIsland* island = IslandAt(screen, a[1]); island != nullptr) island->Show();
        else return -5;
        return 0;
    case 5: {   // destroy the k-th island (the deleting destructor for a flag above 0). The screen's current and last
                // islands are taken off it first, as the scripts never leave them dangling.
        CLBIsland* island = IslandAt(screen, a[1]);
        if (island == nullptr) return -5;
        if (screen->mCurrent == island) screen->mCurrent = nullptr;
        if (screen->mLast == island) screen->mLast = nullptr;
        if (a[2] > 0) delete island;
        else island->~CLBIsland();
        return 0;
    }
    case 6: screen->ResetIslandLoopingSounds(); return 0;
    case 7: screen->SetCurrentIsland(a[1] < 0 ? nullptr : IslandAt(screen, a[1])); return 0;
    case 8: screen->EnterCurrentIsland(); return 0;
    case 9: screen->SelectCurrentIsland(); return 0;
    case 10: screen->StartCurrentIslandZoom(); return 0;
    case 11:   // the list must not be empty (the original reads its first node) or hold a NULL
        if (screen->mIslands->mCount <= 0 || screen->mIslands->mHead == nullptr || HasNullEntry(screen)) return -5;
        screen->ResetCurrentIsland(a[1]);
        return 0;
    case 12: screen->ResetAdvisorTimeout(); return 0;
    case 13: {   // the park records must exist for every island when a player is logged in
        if (HasNullEntry(screen) || SignsWouldFail(screen, false)) return -5;
        screen->UpdateSigns();
        return 0;
    }
    case 14: screen->GotoNextIsland(); return 0;
    case 15: screen->GotoPreviousIsland(); return 0;
    case 16: screen->WindowMessagesFn(a[1], a[2], a[3]); return 0;
    case 17:   // state 2 plays an animation of the current island's mesh
        if (screen->mCameraState == 2 && screen->mCurrent == nullptr) return -5;
        return screen->ProcessCancel();
    case 19: screen->mCameraState = a[1]; return 0;
    case 20: screen->mCurrent = a[1] < 0 ? nullptr : IslandAt(screen, a[1]); return 0;
    case 21: {   // ParseThemeDescriptor: a[1] park, a[2] order, a[3] and a[4] gate names, a[5] the file needs a last island
        if ((a[5] != 0 && screen->mLast == nullptr) || HasNullEntry(screen)) return -5;
        const CFSOnlinePark* park = a[1] != 0 ? reinterpret_cast<const CFSOnlinePark*>(gOnlineParks[(a[1] - 1) & 1]) : nullptr;
        const char16_t* gate1 = a[3] != 0 ? gSigns[(a[3] - 1) & 1] : nullptr;
        const char16_t* gate2 = a[4] != 0 ? gSigns[(a[4] - 1) & 1] : nullptr;
        screen->ParseThemeDescriptor(texts, park, a[2], gate1, gate2);
        return 0;
    }
    case 22:   // Show reads every theme's islands and updates the signs: the same rule as UpdateSigns, over the themes too
        if (HasNullEntry(screen) || SignsWouldFail(screen, true)) return -5;
        static_cast<CLBScreenComponent*>(screen)->Show();
        return 0;
    case 23: {   // Hide, when the screen is shown, destroys the islands: the current and last islands go with them
        const bool wasVisible = screen->mVisible != 0;
        static_cast<CLBScreenComponent*>(screen)->Hide();
        if (wasVisible) {
            screen->mCurrent = nullptr;
            screen->mLast = nullptr;
        }
        return 0;
    }
    case 24:
        if (CLBIsland* island = IslandAt(screen, a[1]); island != nullptr) {
            island->mSound1 = a[2];
            island->mSound2 = a[3];
        } else {
            return -5;
        }
        return 0;
    case 32:   // a NULL entry into an empty list
        if (screen->mIslands->mCount != 0) return -5;
        screen->Register(nullptr);
        return 0;
    case 33:   // an island's "shown" word
        if (CLBIsland* island = IslandAt(screen, a[1]); island != nullptr) island->mShown = a[2];
        else return -5;
        return 0;
    case 30: screen->mLast = a[1] < 0 ? nullptr : IslandAt(screen, a[1]); return 0;
    case 31: screen->mVisible = a[1]; return 0;
    case 34: {   // ResetIslandLoopingSounds with no list (a screen never has none; the code tests for it)
        TbPtrList<CLBIsland>* list = screen->mIslands;
        screen->mIslands = nullptr;
        screen->ResetIslandLoopingSounds();
        screen->mIslands = list;
        return 0;
    }
    }
    return -99;
}

// The state: the view's words, the online system, the lobby's words, the user info, the master save, the
// theme string; then each live screen (its words in the order of the Mac offsets 0x14..0xe4, its islands
// in list order with their words). The islands the code made itself get their labels here, in list order,
// so that their destruction shows in the calls (the fixed objects' labels are below 100).
int32_t tpw_lr_state(int64_t* out) {
    for (int i = 0; i < kScreens; ++i) {
        if (!gScreenAlive[i]) continue;
        for (const auto* node = gScreens[i]->mIslands->mHead; node != nullptr; node = node->mNext)
            if (node->mData != nullptr && lab(node->mData) == -1) newlab(node->mData);
    }
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
    *p++ = gLobby->mStarted;
    *p++ = gLobby->mState;
    *p++ = Hash(gLobbyIslandTheme.mData);
    *p++ = TheGameType().mType;
    *p++ = gUserInfo != nullptr ? 1 + static_cast<int64_t>(gUserInfo->mGameUser) : 0;
    *p++ = lab(gUserInfo);
    *p++ = lab(gMasterSave);
    for (int i = 0; i < kScreens; ++i) {
        if (!gScreenAlive[i]) continue;
        const CLBScreenComponentThemeIslands* s = gScreens[i];
        *p++ = i;
        *p++ = s->mVisible;
        *p++ = s->mIslands->mCount;
        int32_t n = 0;
        for (const auto* node = s->mIslands->mHead; node != nullptr && n < 40; node = node->mNext, ++n) {
            const CLBIsland* island = node->mData;
            *p++ = n;
            *p++ = lab(island);
            if (island == nullptr) continue;   // (a NULL entry)
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
        *p++ = ListIndex(s, s->mCurrent);
        *p++ = ListIndex(s, s->mLast);
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
    return static_cast<int32_t>(p - out);
}

// LB_ReadTbUniStringUntilNext on `text` from `start`: the characters to `out` (16-bit each, \0 ended), the
// cursor's new offset.
int32_t tpw_lr_read_uni(char* text, int32_t start, int32_t delimiter, uint16_t* out) {
    char* cursor = text + start;
    const TbDynamicStringTemplate<char16_t> result(LB_ReadTbUniStringUntilNext(&cursor, static_cast<char>(delimiter)));
    std::size_t n = 0;
    for (; result.mData != nullptr && result.mData[n] != 0; ++n) out[n] = result.mData[n];
    out[n] = 0;
    return static_cast<int32_t>(cursor - text);
}
}
