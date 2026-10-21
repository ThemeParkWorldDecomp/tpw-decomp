// Test entry points for the game object and the theme scan (library unit_Game: Game.cpp,
// FS_MasterSave.cpp, with the real default names of TPGlobals.cpp, the real bullfrog
// strings and GlobalOrder.cpp's two objects). The resource classes, the resource manager
// and the disc storage are fakes that record what they are given.
#include <cstdio>
#include <cstring>
#include <new>

#include "FS_MasterSave.h"
#include "Clock.h"
#include "FS_System.h"
#include "Game.h"
#include "Mouse.h"
#include "SkyObjects.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPEngine.h"
#include "TPString.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "UIInterface.h"

using namespace calllog;
using String = TbDynamicStringTemplate<char>;

// The clock is a recording fake here; its time fields are not used by this unit.
CFixedTime::CFixedTime() {}
CVariableTime::CVariableTime() {}
CClock gClock;
UIInterface gUIInterface;
static void PauseEvent(const char* name, const void* object = nullptr) {
    if (object != nullptr) ev(name, {lab(object), gGame.mF1C, gGame.mF20});
    else ev(name, {gGame.mF1C, gGame.mF20});
}
void CClock::Pause() { PauseEvent("ClockPause", this); }
void CClock::UnPause() { PauseEvent("ClockUnPause", this); }
void UIInterface::Pause() { PauseEvent("UIPause", this); }
void UIInterface::UnPause() { PauseEvent("UIUnPause", this); }
void SOUND_Pause() { PauseEvent("SoundPause"); }
void SOUND_UnPause() { PauseEvent("SoundUnPause"); }
void ADV_Pause() { PauseEvent("AdvisorPause"); }
void ADV_UnPause() { PauseEvent("AdvisorUnPause"); }
void SOUND_SetPauseOn3DSoundsOnly(int32_t on) { ev("Sounds3DOnly", {on, gGame.mF1C, gGame.mF20}); }

// String members the unit's sources reference and nothing here reaches.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- The main app's own helpers (FS_System.cpp, TPString.cpp; tested there) ---------------
void FS_AddDirectorySeparator(String& path) {
    const int32_t length = path.Length();
    if (length == 0 || path.mData[length - 1] != ':') path += gTPGlobals.mSeparator;
}
bool FS_IsSubDirectory(const bfl::FileSystem::CFileInfo& info) {
    return (info.mAttributes & 0x10) != 0 && info.mName[0] != '.';
}
static int Lower(int c) { return c >= 'A' && c <= 'Z' ? c + 0x20 : c; }
bool TPStrGtr(const String& a, const String& b) {
    const char* x = a.mData;
    const char* y = b.mData;
    for (;; ++x, ++y) {
        const int p = Lower(static_cast<unsigned char>(*x)), q = Lower(static_cast<unsigned char>(*y));
        if (p != q) return p > q;
        if (p == 0) return false;
    }
}

// The gate names (src/game/Game.cpp, unit w59; tested in test_LobbyLeaves3) reach the ride database and the unicode compare;
// nothing in this unit calls them.
CRideDatabase gRideDatabase;
void CRideDatabase::GetSignNameA(uint16_t, char16_t*, uint32_t) {}
void CRideDatabase::GetSignNameB(uint16_t, char16_t*, uint32_t) {}
bool TPUniStrEq(const TbDynamicStringTemplate<char16_t>&, const TbDynamicStringTemplate<char16_t>&) { return false; }

// ---- Resources and their manager ------------------------------------------------------------
CTPResourceManager::CTPResourceManager() { ev("Manager", {newlab(this)}); }
CTPResourceManager::~CTPResourceManager() { ev("~Manager", {lab(this)}); }
int32_t CTPResourceManager::RegisterResource(CTPBaseResource* resource) {
    ev("Register", {lab(this), lab(resource)});
    return 1;
}
CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const String&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const String&, bfl::FileSystem::CFileStorage&, uint32_t) {
    return 0;
}
#define RESOURCE(Class, tag)                                                                          \
    Class::Class(const String& name, const String& path) : CTPBaseResource(name, path) {              \
        ev(tag, {newlab(this)}, {name.mData, path.mData});                                             \
    }                                                                                                 \
    Class::~Class() {}
// The classes built on the texture resource record under their own names: they raise
// this before their base's constructor runs.
static bool gDerived = false;
CTPTextureResource::CTPTextureResource(const String& name, const String& path) : CTPBaseResource(name, path) {
    if (!gDerived) ev("Texture", {newlab(this)}, {name.mData, path.mData});
    gDerived = false;
}
CTPTextureResource::~CTPTextureResource() {}
CTPIntermediate::~CTPIntermediate() {}
CTPAdvisorResource::CTPAdvisorResource(const String& name, const String& path)
    : CTPIntermediate((gDerived = true, name), path) {
    ev("Advisor", {newlab(this)}, {name.mData, path.mData});
}
CTPAdvisorResource::~CTPAdvisorResource() {}
CTPUIResource::CTPUIResource(const String& name, const String& path)
    : CTPTextureResource((gDerived = true, name), path) {
    ev("UI", {newlab(this)}, {name.mData, path.mData});
}
CTPUIResource::~CTPUIResource() {}
RESOURCE(CTPSoundResource, "Sound")
RESOURCE(CTPFontResource, "Font")
RESOURCE(CTPSpriteResource, "Sprite")
RESOURCE(CTPLanguageResource, "Language")
RESOURCE(CTPFMVResource, "FMV")
#undef RESOURCE
void CTPBaseResource::AddDiscStorage(const String& path) { ev("AddDisc", {lab(this)}, {path.mData}); }
void CTPBaseResource::AddWADStorage(const String& path) { ev("AddWAD", {lab(this)}, {path.mData}); }
void CTPBaseResource::AddAllStorages(const String& path) { ev("AddAll", {lab(this)}, {path.mData}); }
void CTPBaseResource::AddLocalStorage(const String& path) { ev("AddLocal", {lab(this)}, {path.mData}); }
CSkyObjects::~CSkyObjects() { ev("~Sky", {lab(this)}); }
// What CGame::DeInit (unit w62, tested in test_LobbyLeaves4) calls; nothing in this unit's tests does.
CMouse* gMouse;
void CMouse::AssignMouseMode(CMouseMode*) {}
void CGame::ExitLevel() {}

// ---- The disc storage: a listing the test supplies ("name" or "name/" for a folder, one
// per line) ----------------------------------------------------------------------------------
namespace {
char gListing[4096];
const char* gNextEntry;
bool NextEntry(bfl::FileSystem::CFileInfo& info) {
    if (gNextEntry == nullptr || *gNextEntry == '\0') return false;
    const char* end = std::strchr(gNextEntry, '\n');
    size_t length = end != nullptr ? static_cast<size_t>(end - gNextEntry) : std::strlen(gNextEntry);
    info.mAttributes = 0x20;
    if (length != 0 && gNextEntry[length - 1] == '/') {
        info.mAttributes = 0x10;
        --length;
    }
    std::memcpy(info.mNameBuffer, gNextEntry, length);
    info.mNameBuffer[length] = '\0';
    info.mName = info.mNameBuffer;
    gNextEntry = end != nullptr ? end + 1 : "";
    return true;
}
}  // namespace
bfl::FileSystem::CFileStorage::CFileStorage() {}
bfl::FileSystem::CFileStorage::~CFileStorage() {}
TbError bfl::FileSystem::CDiscFileStorage::Open(const char* path) {
    ev("Open", {}, {path});
    return TbError(static_cast<int32_t>(ret("Open", 0)));
}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    ev("FindFirst", {}, {pattern});
    gNextEntry = gListing;
    return TbError(NextEntry(info) ? 0 : -1);
}
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo& info) {
    ev("FindNext");
    return TbError(NextEntry(info) ? 0 : -1);
}
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(-1); }
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(String&, const String&) { return TbError(0); }
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }

namespace {
// The themes of `game`, one per line, and its other fields.
void Describe(CGame& game, int64_t* out, char* themes) {
    themes[0] = '\0';
    int32_t walked = 0;
    for (auto* node = game.mThemes.mHead; node != nullptr && walked < 64; node = node->mNext, ++walked) {
        std::strcat(themes, node->mData->mData);
        std::strcat(themes, "\n");
    }
    out[0] = game.mCurrentTheme;
    out[1] = game.mRestart;
    out[2] = game.mF1C;
    out[3] = game.mF20;
    out[4] = game.mThemes.mCount;
    out[5] = walked;
    out[6] = lab(game.mResourceManager);
    out[7] = game.mSkyObjects != nullptr;
    out[8] = game.mF3C;
    out[9] = game.mThemes.mTail != nullptr && game.mThemes.mTail->mNext == nullptr ? 1 : game.mThemes.mTail == nullptr ? 0 : -1;
}
void SetThemes(const char* names) {
    gGame.ClearThemeNames();
    while (*names != '\0') {
        const char* end = std::strchr(names, '\n');
        const size_t length = end != nullptr ? static_cast<size_t>(end - names) : std::strlen(names);
        char name[128];
        std::memcpy(name, names, length);
        name[length] = '\0';
        // Straight onto the tail: the order is the test's, not AddThemeName's.
        gGame.mThemes.AddTail(new String(name));
        names = end != nullptr ? end + 1 : names + length;
    }
}
}  // namespace

extern "C" {

// Recording must be off: the game object's themes become `names` (one per line).
void tpw_game_set_themes(const char* names) { SetThemes(names); }
void tpw_game_set_listing(const char* listing) {
    std::strncpy(gListing, listing, sizeof gListing - 1);
    gListing[sizeof gListing - 1] = '\0';
    gNextEntry = nullptr;
}

// op 1: a new game object (the default name is `text`); 2: destroy one (a[0]: with sky
// objects); 3: InitResources; 4: InitLocalSoundResources; 5: AddThemeName(text);
// 6: ClearThemeNames; 7: FS_ScanThemes(text); 8/9: Pause/Unpause with seeded pause
// state (mode, paused, sound mode, reason, all sounds, repetition count). State comes back in
// `out` (10 numbers) and `textOut` (its themes, and for op 1 the current theme's name).
int32_t tpw_game_op(int32_t op, const int32_t* a, const char* text, int64_t* out, char* textOut, char* nameOut) {
    nameOut[0] = '\0';
    int32_t result = 0;
    if (op == 1 || op == 2) {
        const String saved = gTPGlobals.mSearchPath;
        gTPGlobals.mSearchPath = text;
        alignas(CGame) static unsigned char block[sizeof(CGame)];
        std::memset(block, 0xa5, sizeof block);
        auto* game = new (block) CGame;
        gTPGlobals.mSearchPath = saved;
        if (op == 1) {
            std::strcpy(nameOut, game->mCurrentThemeName.mData);
            Describe(*game, out, textOut);
            // (left alive: the fake manager it made stays labelled for the comparison)
            return 0;
        }
        game->AddThemeName(String("one"));
        game->AddThemeName(String("two"));
        if (a[0]) {
            game->mSkyObjects = static_cast<CSkyObjects*>(::operator new(sizeof(CSkyObjects)));
            newlab(game->mSkyObjects);
        }
        game->~CGame();
        textOut[0] = '\0';
        for (int i = 0; i < 10; ++i) out[i] = 0;
        return 0;
    }
    // The game object gets a stand-in manager for the call (label 3); its own goes back
    // afterwards, since the object's destructor deletes it when the library unloads.
    static char manager;
    CTPResourceManager* own = gGame.mResourceManager;
    gGame.mResourceManager = reinterpret_cast<CTPResourceManager*>(&manager);
    tpw_cl_label(3, &manager);
    const int32_t savedMode = gGame.mF3C, savedPaused = gGame.mF1C;
    const uint8_t savedSounds = gGame.mF20;
    if (op == 8 || op == 9) {
        gGame.mF3C = a[0];
        gGame.mF1C = a[1];
        gGame.mF20 = static_cast<uint8_t>(a[2]);
        tpw_cl_label(4, &gClock);
        tpw_cl_label(5, &gUIInterface);
        for (int32_t i = 0; i < a[5]; ++i) {
            if (op == 8) gGame.Pause(static_cast<CGame::ePauseFlag>(a[3]), a[4] != 0);
            else gGame.Unpause();
        }
    }
    switch (op) {
    case 3: gGame.InitResources(); break;
    case 4: gGame.InitLocalSoundResources(); break;
    case 5: gGame.AddThemeName(String(text)); break;
    case 6: gGame.ClearThemeNames(); break;
    case 7: result = FS_ScanThemes(String(text)).mValue; break;
    }
    Describe(gGame, out, textOut);
    gGame.mF3C = savedMode;
    gGame.mF1C = savedPaused;
    gGame.mF20 = savedSounds;
    gGame.mResourceManager = own;
    return result;
}

}  // extern "C"

extern "C" void tpw_game_getters(int32_t theme, int32_t* out) {
    CGame game;
    game.mCurrentTheme = theme;
    game.mRestart = 42;
    out[0] = game.GetCurrentTheme();
    out[1] = &game.GetCurrentThemeName() == &game.mCurrentThemeName;
}
