// Test library for the way out of the lobby (unit w104; libtpw_unit_LobbyExit.dylib, tests/test_LobbyExit.py):
//   src/game/Game.cpp   CGame::SetCurrentTheme (two), SetCurrentThemeName, ConverttoCurrentTheme
//   src/game/TPCamera.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_islandLobbyController.cpp, src/game/ui/ui_main.cpp       KeyParticlePOUFFE, DeInitLobbyUI, UIInterface::DeInitLobby, IsRideFirstPersonRide
//   src/game/HLSprite.cpp  ESP_RemoveSprites
//   src/game/online/lobby_globe.cpp  the online globe's destructor, Close and the two list helpers
//
// Real: those, the rectangle (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp) and the light (src/game/Light.cpp). Fakes that record the same event on both sides: the
// strings' copy constructor, destructor and virtual assignment (the other string members are unrecorded and functional), the
// master save's constructor and ChangeCurrentTheme, CGame::GetCurrentThemeName, _stricmp, UI_DisableHelpText, CMapWho::Unlink,
// InterfaceWindow::GetWindow and GetPosition (which writes the rectangle from the case's queue), PAR_InitEmitter, SOUND_SendEvent,
// CSpriteFileIO::Free, CSpriteList::Close, the screen base class's destructor, and the entries and lists of the globe (their
// destructors). The unit file includes calllog.cpp itself and keeps every block the program frees, so that a test can read what
// a freed object held (ESP_RemoveSprites clears the file word of a set it then deletes) and no address is reused.
//
// One entry point per operation: a description of numbers in, a dump of numbers out, the events in the call log. The Mac-layout
// objects of a case are built in emulator memory by the test module.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
bool KeepFreed(void*, bool) { return true; }
}  // namespace
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Game.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPSound.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "sprite_engine.h"
#include "tb_starfield.h"
#include "ui_main.h"

using namespace calllog;

namespace {

// The pointer as the call log sees it, whatever the compiler knows about `this`: a window or a map may be NULL here.
const void* Launder(const void* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

template<class Char> std::size_t TextLength(const Char* text) {
    std::size_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}

template<class Char> void SetText(TbDynamicStringTemplate<Char>& s, const Char* text, std::size_t n) {
    Char* data = static_cast<Char*>(std::malloc((n + 1) * sizeof(Char)));
    if (n != 0) std::memcpy(data, text, n * sizeof(Char));
    data[n] = 0;
    s.mData = data;
    s.mLength = static_cast<int32_t>(n);
}

// The text of a string as an event records it (only the narrow strings are recorded).
const char* Text(const char* text) { return text; }

}  // namespace

// ---- the strings: the copy constructor, the destructor and the virtual assignment record; everything else is functional and quiet
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    const Char empty[1] = {0};
    SetText(*this, empty, 0);
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char* text) : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, text, TextLength(text));
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) : mGrowBy(0) {
    ev("String.copy", {lab(Launder(&other))}, {Text(other.mData)});
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, other.mData, static_cast<std::size_t>(other.mLength));
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbStringBase<Char>& other) : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, other.mData, static_cast<std::size_t>(other.mLength));
}
template<class Char> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() { ev("String.dtor", {lab(this)}); }
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>& other) {
    SetText(*this, other.mData, static_cast<std::size_t>(other.mLength));
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate& other) {
    ev("String.assign", {lab(this), lab(&other)}, {Text(other.mData)});
    SetText(*this, other.mData, static_cast<std::size_t>(other.mLength));
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char* text) {
    SetText(*this, text, TextLength(text));
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>&) {
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) { return *this; }
template class TbDynamicStringTemplate<char>;

// ---- globals of other files that the unit owns here
// The game is only an address here (its members are built in place): raw storage under the symbol's name.
#ifdef __APPLE__
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("_gGame");
extern unsigned char gHLListStorage[16 * sizeof(CSpriteList)] __asm__("_gHLSpriteLists");
extern unsigned char gGlobalListStorage[sizeof(CSpriteList)] __asm__("_gESPGlobalList");
#else
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("gGame");
extern unsigned char gHLListStorage[16 * sizeof(CSpriteList)] __asm__("gHLSpriteLists");
extern unsigned char gGlobalListStorage[sizeof(CSpriteList)] __asm__("gESPGlobalList");
#endif
alignas(16) unsigned char gGameStorage[sizeof(CGame)] = {};
alignas(16) unsigned char gHLListStorage[16 * sizeof(CSpriteList)] = {};
alignas(16) unsigned char gGlobalListStorage[sizeof(CSpriteList)] = {};
CFSGlobalMasterSave* gMasterSave;
InterfaceWindow* gIslandLobbyController;
int32_t gLobbyThunderBank;
uint32_t gFirstPersonFlags;
tag_sRideAnimSet* gFirstPersonRideSet;
uint32_t gFirstPersonRideInstance;
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
int32_t gESPSpriteCount;
CLight gUILight;
CLight* gev_LightChain;
CMapWho* gei_MapWho;

namespace {

enum Label : int32_t {
    kGame = 5, kMaster = 6, kMapWho = 7, kLink = 8, kController = 9, kFound = 10, kGlobe = 11, kGlobeList = 12, kCityList = 13,
    kName = 20,        // 20..29: the theme names; 30: the argument's name
    kArgName = 30,
    kSprite = 50,      // 50..79
    kFile = 80,        // 80..109
    kSpriteList = 120, // 120..135, 136 the global list
    kEntry = 150,      // 150..: the globe's entries, then the city flags
};

// The names the cases choose from (the test module has the same table).
const char* const kNames[] = {"fantasy", "hallow", "jungle", "space", "Fantasy", "HALLOW", "", "jungle ", "JUNGLE", "zzz", "start"};
constexpr int kNameCount = sizeof(kNames) / sizeof(kNames[0]);

CGame* Game() { return reinterpret_cast<CGame*>(gGameStorage); }

void ClearHeapCounts() {
    for (int32_t& count : tpw_cl_heap) count = 0;
}

}  // namespace

// ---- the fakes
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave.ctor", {newlab(this)}); }
void CFSGlobalMasterSave::ChangeCurrentTheme(const TbDynamicStringTemplate<char>& name) {
    ev("ChangeCurrentTheme", {lab(this), lab(&name)}, {name.mData});
}
const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return mCurrentThemeName;
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : static_cast<unsigned char>(*a);
        const int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : static_cast<unsigned char>(*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

namespace {
GameType gCtorType;   // what the game type's constructor makes (the Mac's reads the options; the case says)
}
CGameType::CGameType() : mType(gCtorType) {}

void UI_DisableHelpText() { ev("UI_DisableHelpText"); }
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    ev("CMapWho.Unlink", {lab(Launder(this)), lab(link), list});
    return 1;
}
CMapLinkBase::~CMapLinkBase() {}

namespace {
alignas(16) unsigned char gWindowBytes[2][0x140];
InterfaceWindow* Window(int32_t k) { return reinterpret_cast<InterfaceWindow*>(gWindowBytes[k]); }
InterfaceWindow* gFoundWindow;   // what GetWindow answers
}  // namespace
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return gFoundWindow;
}
// Writes the rectangle from the case's queue "Rect" (left, top, right, bottom); this may be NULL.
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(Launder(this)), subtract});
    rect->mLeft = static_cast<int16_t>(ret("Rect", 0));
    rect->mTop = static_cast<int16_t>(ret("Rect", 0));
    rect->mRight = static_cast<int16_t>(ret("Rect", 0));
    rect->mBottom = static_cast<int16_t>(ret("Rect", 0));
    return 1;
}
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return 0;
}
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}
int32_t CSpriteFileIO::Free() {
    ev("CSpriteFileIO.Free", {lab(Launder(this))});
    return 1;
}
int32_t CSpriteList::Close() {
    ev("CSpriteList.Close", {lab(this)});
    return 1;
}

// The screen base class: only the destructor records; the online globe's constructor is empty here (the test sets the fields).
CLBScreenComponent::CLBScreenComponent() : mVisible(0) {}
CLBScreenComponent::~CLBScreenComponent() { ev("Screen.dtor", {lab(this)}); }
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return mVisible; }
CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/game/online/lobby_globe.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentOnlineGlobe::CreateCities(TbPtrList<CFSOnlineCity>*) {}
CLBGlobe::~CLBGlobe() {}
CLBGlobeCityFlag::~CLBGlobeCityFlag() {}

namespace {

// A globe as the game makes them: one of the concrete screens. Its other virtuals are empty.
struct TestGlobe : CLBScreenComponentOnlineGlobe {
    void Init() override {}
    void Tick() override {}
    void WindowMessagesFn(int32_t, int32_t, int32_t) override {}
};
// The entries and the lists record their destruction.
struct GlobeEntry : CLBGlobe {
    ~GlobeEntry() override { ev("Globe.dtor", {lab(this)}); }
};
struct CityEntry : CLBGlobeCityFlag {
    ~CityEntry() override { ev("CityFlag.dtor", {lab(this)}); }
};
struct GlobeList : TbTList<CLBGlobe*, CLBGlobe*> {
    ~GlobeList() override { ev("GlobeList.dtor", {lab(this)}); }
};
struct CityList : TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*> {
    ~CityList() override { ev("CityList.dtor", {lab(this)}); }
};

// ---------------------------------------------------------------------------------------------------- the theme setters

alignas(16) unsigned char gMasterStore[sizeof(CFSGlobalMasterSave)];
alignas(16) unsigned char gNameStore[kNameCount][sizeof(TbDynamicStringTemplate<char>)];
alignas(16) unsigned char gArgStore[sizeof(TbDynamicStringTemplate<char>)];

int32_t NameId(const char* text) {
    for (int32_t i = 0; i < kNameCount; ++i)
        if (std::strcmp(kNames[i], text) == 0) return i;
    return -1;
}

// description: the operation (0 SetCurrentTheme(int), 1 SetCurrentTheme(name), 2 SetCurrentThemeName, 3 ConverttoCurrentTheme), the
//   argument (the index, or the theme number), the current index, whether the master save exists, the argument name's id, the number
//   of names, then their ids
// dump: the current index, the id of the current name's text, the master save's label, the heap's new and delete counts
int32_t RunTheme(const int64_t* d, int64_t* out) {
    const int32_t op = static_cast<int32_t>(d[0]);
    const int32_t arg = static_cast<int32_t>(d[1]);
    const int32_t count = static_cast<int32_t>(d[5]);
    tpw_cl_on = 0;
    CGame* game = Game();
    new (&game->mCurrentThemeName) TbDynamicStringTemplate<char>(kNames[10]);
    new (&game->mThemes) TbPtrList<TbDynamicStringTemplate<char>>;
    game->mCurrentTheme = static_cast<int32_t>(d[2]);
    tpw_cl_label(kGame, game);
    for (int32_t i = 0; i < count; ++i) {
        auto* name = new (gNameStore[i]) TbDynamicStringTemplate<char>(kNames[d[6 + i]]);
        game->mThemes.AddTail(name);
        tpw_cl_label(kName + i, name);
    }
    auto* argName = new (gArgStore) TbDynamicStringTemplate<char>(kNames[d[4]]);
    tpw_cl_label(kArgName, argName);
    if (d[3] != 0) {
        gMasterSave = reinterpret_cast<CFSGlobalMasterSave*>(gMasterStore);
        tpw_cl_label(kMaster, gMasterSave);
    } else {
        gMasterSave = nullptr;
    }
    ClearHeapCounts();

    tpw_cl_on = 1;
    switch (op) {
    case 0: game->SetCurrentTheme(arg); break;
    case 1: game->SetCurrentTheme(*argName); break;
    case 2: game->SetCurrentThemeName(); break;
    case 3: ConverttoCurrentTheme(arg); break;
    }
    tpw_cl_on = 0;

    out[0] = game->mCurrentTheme;
    out[1] = NameId(game->mCurrentThemeName.mData);
    out[2] = lab(gMasterSave);
    out[3] = tpw_cl_heap[0];
    out[4] = tpw_cl_heap[2];
    gMasterSave = nullptr;
    return 5;
}

// ---------------------------------------------------------------------------------------------------- the lobby's exit

// description: the first-person flags, whether a ride set is current, the remembered instance, the argument
// dump: the answer
int32_t RunRide(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    gFirstPersonFlags = static_cast<uint32_t>(d[0]);
    gFirstPersonRideSet = d[1] != 0 ? reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(0x1000)) : nullptr;
    gFirstPersonRideInstance = static_cast<uint32_t>(d[2]);
    out[0] = IsRideFirstPersonRide(static_cast<uint32_t>(d[3]));
    return 1;
}

alignas(16) unsigned char gMapStorage[256];

// description: the operation (0 UIInterface::DeInitLobby, 1 DeInitLobbyUI), whether the map exists
int32_t RunDeInit(const int64_t* d, int64_t* out) {
    (void)out;
    tpw_cl_on = 0;
    gei_MapWho = d[1] != 0 ? reinterpret_cast<CMapWho*>(gMapStorage) : nullptr;
    tpw_cl_label(kMapWho, gMapStorage);
    tpw_cl_label(kLink, static_cast<CMapLinkBase*>(&gUILight));
    tpw_cl_on = 1;
    if (d[0] == 0) {
        UIInterface ui;
        ui.DeInitLobby();
    } else {
        DeInitLobbyUI();
    }
    tpw_cl_on = 0;
    return 0;
}

// description: whether the island controller exists, the game type, whether the type object exists already (the host cannot unmake
//   it), whether GetWindow finds the window, the thunder bank
// queue: Rect (left, top, right, bottom of GetPosition)
int32_t RunKey(const int64_t* d, int64_t* out) {
    (void)out;
    tpw_cl_on = 0;
    gCtorType = static_cast<GameType>(d[1]);
    TheGameType().mType = static_cast<GameType>(d[1]);
    gIslandLobbyController = d[0] != 0 ? Window(0) : nullptr;
    gFoundWindow = d[3] != 0 ? Window(1) : nullptr;
    gLobbyThunderBank = static_cast<int32_t>(d[4]);
    tpw_cl_label(kController, Window(0));
    tpw_cl_label(kFound, Window(1));
    tpw_cl_on = 1;
    KeyParticlePOUFFE();
    tpw_cl_on = 0;
    return 0;
}

// ---------------------------------------------------------------------------------------------------- ESP_RemoveSprites

// description: gESPSpriteCount, the sets made (at least the count and one more), then per set: the loaded byte, whether it has a file
// dump: the count, per set whether its slot is used and whether its file word is not NULL, the heap's new and delete counts
int32_t RunRemove(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    const int32_t sets = static_cast<int32_t>(d[1]);
    for (auto& slot : gESPSprites) slot = nullptr;
    for (int32_t i = 0; i < sets; ++i) {
        auto* sprite = static_cast<ESP_CSprite*>(::operator new(sizeof(ESP_CSprite)));
        std::memset(static_cast<void*>(sprite), 0xa5, sizeof(ESP_CSprite));
        sprite->mLoaded = static_cast<uint8_t>(d[2 + 2 * i]);
        sprite->mFile = d[3 + 2 * i] != 0 ? new CSpriteFileIO : nullptr;
        gESPSprites[i] = sprite;
        tpw_cl_label(kSprite + i, sprite);
        if (sprite->mFile != nullptr) tpw_cl_label(kFile + i, sprite->mFile);
    }
    gESPSpriteCount = static_cast<int32_t>(d[0]);
    for (int32_t k = 0; k < 16; ++k) tpw_cl_label(kSpriteList + k, &gHLSpriteLists[k]);
    tpw_cl_label(kSpriteList + 16, &gESPGlobalList);
    ClearHeapCounts();

    tpw_cl_on = 1;
    ESP_RemoveSprites();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = gESPSpriteCount;
    for (int32_t i = 0; i < sets; ++i) {
        out[n++] = gESPSprites[i] != nullptr;
        out[n++] = gESPSprites[i] != nullptr && gESPSprites[i]->mFile != nullptr;
    }
    out[n++] = tpw_cl_heap[0];
    out[n++] = tpw_cl_heap[2];
    return n;
}

// ---------------------------------------------------------------------------------------------------- the online globe

alignas(16) unsigned char gGlobeStore[sizeof(TestGlobe)];

// description: the operation (0 destructor, 1 DeleteGlobeListEntries, 2 DeleteCityListEntries, 3 Close, 4 destructor of NULL), the
//   destructor's flag, the globes, the city flags, then one kind per entry (0 a NULL entry, 1 an object): the globes', then the flags'
// dump: the heap's new and delete counts, then (not after a destructor) the lists' counts
int32_t RunGlobe(const int64_t* d, int64_t* out) {
    const int32_t op = static_cast<int32_t>(d[0]);
    const int32_t flag = static_cast<int32_t>(d[1]);
    const int32_t globes = static_cast<int32_t>(d[2]);
    const int32_t flags = static_cast<int32_t>(d[3]);
    tpw_cl_on = 0;
    TestGlobe* object = flag > 0 ? new TestGlobe : new (gGlobeStore) TestGlobe;
    auto* globeList = new GlobeList;
    auto* cityList = new CityList;
    object->mGlobes = globeList;
    object->mCityFlags = cityList;
    tpw_cl_label(kGlobe, object);
    tpw_cl_label(kGlobeList, globeList);
    tpw_cl_label(kCityList, cityList);
    for (int32_t i = 0; i < globes; ++i) {
        CLBGlobe* entry = d[4 + i] != 0 ? new GlobeEntry : nullptr;
        globeList->AddTail(entry);
        if (entry != nullptr) tpw_cl_label(kEntry + i, entry);
    }
    for (int32_t i = 0; i < flags; ++i) {
        CLBGlobeCityFlag* entry = d[4 + globes + i] != 0 ? new CityEntry : nullptr;
        cityList->AddTail(entry);
        if (entry != nullptr) tpw_cl_label(kEntry + 40 + i, entry);
    }
    ClearHeapCounts();

    CLBScreenComponentOnlineGlobe* globe = object;
    tpw_cl_on = 1;
    switch (op) {
    case 0:
        if (flag > 0) delete globe;
        else object->~TestGlobe();   // (the abstract class's own complete-object destructor is a trap; TestGlobe adds nothing to it)
        break;
    case 1: globe->DeleteGlobeListEntries(); break;
    case 2: globe->DeleteCityListEntries(); break;
    case 3: globe->CLBScreenComponentOnlineGlobe::Close(); break;
    case 4: {
        CLBScreenComponentOnlineGlobe* none = nullptr;
        delete none;
        break;
    }
    }
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = tpw_cl_heap[0];
    out[n++] = tpw_cl_heap[2];
    if (op != 0) {
        out[n++] = globeList->mCount;
        out[n++] = cityList->mCount;
        out[n++] = object->mGlobes == globeList;
        out[n++] = object->mCityFlags == cityList;
    }
    return n;
}

}  // namespace

extern "C" {

int32_t tpw_lx_theme(const int64_t* d, int64_t* out) { return RunTheme(d, out); }
int32_t tpw_lx_ride(const int64_t* d, int64_t* out) { return RunRide(d, out); }
int32_t tpw_lx_deinit(const int64_t* d, int64_t* out) { return RunDeInit(d, out); }
int32_t tpw_lx_key(const int64_t* d, int64_t* out) { return RunKey(d, out); }
int32_t tpw_lx_remove(const int64_t* d, int64_t* out) { return RunRemove(d, out); }
int32_t tpw_lx_globe(const int64_t* d, int64_t* out) { return RunGlobe(d, out); }

}  // extern "C"
