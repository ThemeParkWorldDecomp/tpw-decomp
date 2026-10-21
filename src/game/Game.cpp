// Game.cpp: name proven by __sinit_Game_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "analyser.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "Game.h"
#include "HLSprite.h"
#include "HoardingsBalance.h"
#include "LevelInit.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/macdoze.h"
#include "SkyObjects.h"
#include "StaffDB.h"
#include "tb_resource_display.h"
#include "tb_sky.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPResource.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldThings.h"
#include <cstdint>
#include <cstring>
#include <new>

// Game.cpp range of the main app (SimTheme Park 2.04b3): the game object's construction,
// resources and theme names. (The command line, from the same range, is in
// CommandLine.cpp.) PARTIAL: the levels, themes, pausing and the rest of CGame are still
// to do.

// ---- CGame ------------------------------------------------------------------------------
// The object is gGame (src/GlobalOrder.cpp). The original's constructor is inline in the
// file's static initialiser, 0x1014b118, which also builds five wide strings this file
// keeps (L"gms.dat" and the theme names L"fantasy", L"hallow", L"jungle", L"space"); those
// come with the functions that use them.
CGame::CGame() {
    mRestart = 0;
    mF1C = 0;
    mF3C = 1;
    mCurrentTheme = -1;
    mCurrentThemeName = gTPGlobals.mSearchPath;
    mF20 = 0;
    mResourceManager = new CTPResourceManager;
    mSkyObjects = nullptr;
}

CGame::~CGame() {
    ClearThemeNames();
    delete mResourceManager;
    delete mSkyObjects;
}

// CGame::InitWorld (main app of SimTheme Park 2.04b3; unit w113).

// 0x10147e34 (136 bytes): creates the world. `this` (the CGame) is never read. The world pointer is stored into gGameWorld BEFORE the
// NULL test, so an allocation that fails overwrites a previous world pointer with NULL (the answer is then 0 and nothing else runs).
// gGameWorld is not cleared when Allocate fails (it cannot: AllocateThingHeap always answers 1).
int32_t CGame::InitWorld() {
    CWorld* const world = new (std::nothrow) CWorld;
    gGameWorld = world;
    if (world == nullptr) return 0;
    if (!gGameWorld->Allocate()) return 0;
    return 1;
}

// CGame::InitTheme and the level it loads (main app, SimTheme Park 2.04b3, Game.cpp; unit w117): InitTheme 0x10147ee4, InitLevel 0x10148028,
// InitLevel_Load 0x1014809c, InitLevel_Initialise 0x101482bc and LoadTheme 0x1014960c.

// 0x102c6c98, 0x102a4d6c, 0x102a4d70, 0x102f9bc8 (see the headers).
uint32_t gGlobalAmbientColour = 0;
int32_t gLevelRangeMinX = 0;
int32_t gLevelRangeMinY = 0;
s3DPoint gWaterStartPos = {0.0f, 0.0f, 0.0f};

namespace {
// The balance colours are ARGB words that the engine takes rotated left by a byte.
uint32_t RotateLeft8(uint32_t value) { return (value >> 24) | (value << 8); }
}  // namespace

// The theme's ride database, the new level, the quick save the command line names and the level's objects. A quick save that loads (or
// not) changes nothing: the answer is 0 only when LoadTheme, SetupNewLevel or InitLevel answered 0. The world is the value of gGameWorld.
int32_t CGame::InitTheme(char* path) {
    if (LoadTheme(path) == 0) return 0;
    if (!gGameWorld->SetupNewLevel(path)) return 0;
    if (gCommandLineOptions.mLoadFile != 0) {
        // (narrow is built first and destroyed last, as in the original)
        TbDynamicStringTemplate<char> narrow(gCommandLineOptions.mFile);
        TbDynamicStringTemplate<char16_t> wide;
        StringToUniString(wide, narrow);
        if (gLoadSave.Load(wide) != 0) DEBUG_MESSAGE("QuickSave Loaded", 0);
    }
    return InitLevel() == 0 ? 0 : 1;
}

int32_t CGame::InitLevel() {
    if (InitLevel_Load() == 0) return 0;
    return InitLevel_Initialise();
}

// The level's objects, sky, sky objects and the two hoardings tables. get_theme_path is called four times (the object file, the sky, and
// each table); the string's data pointer is read again after every append, since an append may move it.
int32_t CGame::InitLevel_Load() {
    TbDynamicStringTemplate<char> path;
    path = get_theme_path();
    path += ":Scape.omp";
    OBJECT_Load(FindFilePath(path.mData, kFileReturnName, nullptr));
    gei_Sky.SkyLoad(get_theme_path());
    // The Mac allocates 0x10 bytes and tests the allocation for NULL (the host's never fails), then stores the pointer and starts the objects.
    if ((gei_Display.mFlags & 0x400000) != 0 && gMainBalance.mValues[kMainBalanceNumSkyObjects].mInteger > 0 && mSkyObjects == nullptr) {
        mSkyObjects = new CSkyObjects;
        mSkyObjects->InitSkyObjects();
    }
    path = get_theme_path();
    FS_AddDirectorySeparator(path);
    path += gTPGlobals.mF40EC;
    if (gHoardingsBalance434C.Load(path.mData, CBalanceTable::kLoadFresh) == 0) return 0;
    path = get_theme_path();
    FS_AddDirectorySeparator(path);
    path += gTPGlobals.mF4100;
    if (gHoardingsBalance4130.Load(path.mData, CBalanceTable::kLoadFresh) == 0) return 0;
    return 1;
}

// The theme's settings out of gMainBalance (value slot = (Mac offset - 8) / 4) into the engine's and the game's globals. It ignores `this`.
// The colour words are rotated left by a byte. The two heightfield offsets are raised to the lowest values the data words give (signed).
// The cell costs are passed to the setters in the order path, queue, kart track, water track (each from its own slot).
int32_t CGame::InitLevel_Initialise() {
    const BalanceValue* values = gMainBalance.mValues;
    const uint32_t fogColour = static_cast<uint32_t>(values[kMainBalanceFogColour].mInteger);
    const uint32_t directionalLight = static_cast<uint32_t>(values[kMainBalanceDirectionalLightLevel].mInteger);
    const uint32_t ambientLight = static_cast<uint32_t>(values[kMainBalanceAmbientLightLevel].mInteger);
    gei_Display.SetFogColor(RotateLeft8(fogColour));
    gGlobalAmbientColour = RotateLeft8(ambientLight);
    gGlobalLightColour = RotateLeft8(directionalLight);
    gHeightfieldRangeExtraX = values[kMainBalanceProcTexXOffset].mInteger;
    gHeightfieldRangeExtraY = values[kMainBalanceProcTexYOffset].mInteger;
    if (gHeightfieldRangeExtraX < gLevelRangeMinX) gHeightfieldRangeExtraX = gLevelRangeMinX;
    if (gHeightfieldRangeExtraY < gLevelRangeMinY) gHeightfieldRangeExtraY = gLevelRangeMinY;
    gGlobalLightDirection.x = values[kMainBalanceLightNormalX].mFloat;
    gGlobalLightDirection.y = values[kMainBalanceLightNormalX + 1].mFloat;
    gGlobalLightDirection.z = values[kMainBalanceLightNormalX + 2].mFloat;
    gWaterStartPos.x = values[kMainBalanceWaterStartX].mFloat;
    gWaterStartPos.y = values[kMainBalanceWaterStartX + 1].mFloat;
    gWaterStartPos.z = values[kMainBalanceWaterStartX + 2].mFloat;
    for (int32_t i = 0; i < 4; ++i) {
        gAdvisorState.mAccessoriesOn[i] = values[kMainBalanceCostumeAddOns + 2 * i].mInteger;
        gAdvisorState.mAccessoriesOff[i] = values[kMainBalanceCostumeAddOns + 2 * i + 1].mInteger;
    }
    LLCC_SystemSetPathCost(values[kMainBalancePathCell].mInteger);
    LLCC_SystemSetQueueCost(values[kMainBalanceQueueCell].mInteger);
    LLCC_SystemSetKartTrackCost(values[kMainBalanceKartTrackCell].mInteger);
    LLCC_SystemSetWaterTrackCost(values[kMainBalanceWaterTrackCell].mInteger);
    return 1;
}

// For each theme folder: its sound effects and speech (found locally) and its music
// (found locally or on the disc), as resources named after the theme. The themes are the
// game object's own whichever object this is called on.
void CGame::InitLocalSoundResources() {
    const TPGlobals& g = gTPGlobals;
    for (auto* node = gGame.mThemes.mHead; node != nullptr;) {
        TbDynamicStringTemplate<char> path;
        TbDynamicStringTemplate<char> name;
        const TbDynamicStringTemplate<char>& theme = *node->mData;
        node = node->mNext;

        path = g.mF4088;
        FS_AddDirectorySeparator(path);
        path += theme;
        FS_AddDirectorySeparator(path);
        path += g.mF4C18;
        name = g.mF4790;
        name += theme;
        CTPBaseResource* resource = new CTPSoundResource(name, path);
        resource->AddLocalStorage(path);
        mResourceManager->RegisterResource(resource);

        path = g.mF4088;
        FS_AddDirectorySeparator(path);
        path += theme;
        FS_AddDirectorySeparator(path);
        path += g.mF4C40;
        name = g.mF47A4;
        name += theme;
        resource = new CTPSoundResource(name, path);
        resource->AddLocalStorage(path);
        mResourceManager->RegisterResource(resource);

        path = g.mF4088;
        FS_AddDirectorySeparator(path);
        path += theme;
        FS_AddDirectorySeparator(path);
        path += g.mF4C68;
        name = g.mF47B8;
        name += theme;
        resource = new CTPSoundResource(name, path);
        resource->AddAllStorages(path);
        mResourceManager->RegisterResource(resource);
    }
}

// Where the game's files are: each resource is a named set of places (a folder, an
// archive of the same name, the same again on the disc). The splash screens are the one
// resource given by a literal disc path.
void CGame::InitResources() {
    const TPGlobals& g = gTPGlobals;
    CTPBaseResource* resource = new CTPTextureResource(g.mF46DC, g.mDataFolder);
    resource->AddAllStorages(g.mDataFolder);
    mResourceManager->RegisterResource(resource);

    resource = new CTPTextureResource(g.mF45C4, g.mLevelsFolder);
    resource->AddLocalStorage(g.mLevelsFolder);
    mResourceManager->RegisterResource(resource);

    resource = new CTPFMVResource(g.mF45EC, g.mF48A8);
    resource->AddAllStorages(g.mF48A8);
    mResourceManager->RegisterResource(resource);

    resource = new CTPTextureResource(TbDynamicStringTemplate<char>("splash"),
                                      TbDynamicStringTemplate<char>("Theme Park:Theme Park Data:data:splash"));
    resource->AddAllStorages(TbDynamicStringTemplate<char>("Theme Park:Theme Park Data:data:splash"));
    mResourceManager->RegisterResource(resource);

    resource = new CTPSoundResource(g.mF4768, g.mF4A9C);
    resource->AddLocalStorage(g.mF4A9C);
    mResourceManager->RegisterResource(resource);

    resource = new CTPSoundResource(g.mF477C, g.mF4AC4);
    resource->AddLocalStorage(g.mF4AC4);
    mResourceManager->RegisterResource(resource);

    resource = new CTPFontResource(g.mF45D8, g.mF4880);
    resource->AddLocalStorage(g.mF4880);
    mResourceManager->RegisterResource(resource);

    resource = new CTPUIResource(g.mF4600, g.mF48BC);
    resource->AddDiscStorage(g.mF48BC);
    resource->AddWADStorage(g.mF48D0);
    mResourceManager->RegisterResource(resource);

    InitLocalSoundResources();

    resource = new CTPSpriteResource(g.mF4664, g.mF48E4);
    resource->AddDiscStorage(g.mF48E4);
    resource->AddWADStorage(g.mF48F8);
    mResourceManager->RegisterResource(resource);

    resource = new CTPTextureResource(g.mF468C, g.mF490C);
    resource->AddDiscStorage(g.mF490C);
    mResourceManager->RegisterResource(resource);

    resource = new CTPLanguageResource(g.mF4678, g.mLanguageFolder);
    resource->AddDiscStorage(g.mLanguageFolder);
    mResourceManager->RegisterResource(resource);

    resource = new CTPTextureResource(g.mF46F0, g.mF4858);
    resource->AddDiscStorage(g.mF4858);
    resource->AddWADStorage(g.mF486C);
    mResourceManager->RegisterResource(resource);

    resource = new CTPAdvisorResource(g.mF463C, g.mF4AEC);
    resource->AddLocalStorage(g.mF4AEC);
    mResourceManager->RegisterResource(resource);

    resource = new CTPTextureResource(g.mF4754, g.mF4A74);
    resource->AddDiscStorage(g.mF4A74);
    resource->AddWADStorage(g.mF4A88);
    mResourceManager->RegisterResource(resource);
}

// Game.cpp range of the main app (SimTheme Park 2.04b3): InitThemeResources (0x10148c3c, 1,708 bytes), which registers the
// resources of the theme a park is built in (src/Game.cpp has InitResources, the ones the game always has).

// Fourteen resources, each a name and a folder. The folder is the park's path (`path`, handed in by the caller) for the first
// one; for the others it is the path, a directory separator and one folder word of gTPGlobals. Every resource is given its
// local storages (the folder and path.wad) and registered with the game's resource manager (this object's, not gGame's;
// the first resource is named after the CURRENT THEME of gGame, whichever object this is called on). The shared texture
// resources go through the manager's second RegisterResource, which keeps them in the manager's shared texture slot.
void CGame::InitThemeResources(const TbDynamicStringTemplate<char>& path) {
    const TPGlobals& g = gTPGlobals;
    TbDynamicStringTemplate<char> folder;
    folder = path;

    // The theme's own textures: the folder is the path as it is (no separator is added).
    CTPBaseResource* resource = new CTPTextureResource(gGame.GetCurrentThemeName(), folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4B78;
    CTPSharedTextureResource* shared = new CTPSharedTextureResource(g.mF4614, folder);
    shared->AddLocalStorage(folder);
    mResourceManager->RegisterResource(shared);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4B8C;
    shared = new CTPSharedTextureResource(g.mF4628, folder);
    shared->AddLocalStorage(folder);
    mResourceManager->RegisterResource(shared);

    // The ride database's lists: rides, features, sideshows, upgrades and shops.
    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BB4;
    resource = new CTPItemListResource(g.mF4650, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BF0;
    resource = new CTPItemListResource(g.mF4740, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BC8;
    resource = new CTPItemListResource(g.mF472C, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4C04;
    resource = new CTPItemListResource(g.mF4704, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BDC;
    resource = new CTPItemListResource(g.mF4718, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4B28;
    resource = new CTPAdvisorResource(g.mF47CC, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF49AC;
    resource = new CTPTerrainResource(g.mF46C8, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF49D4;
    resource = new CTPTextureResource(g.mF47E0, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF49FC;
    resource = new CTPTextureResource(g.mF47F4, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4A24;
    resource = new CTPTextureResource(g.mF4808, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4A4C;
    resource = new CTPTextureResource(g.mF481C, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);
}

// CGame::ClearThemeResources (main app, SimTheme Park 2.04b3, Game.cpp; unit w81).

// 0x10149334 (680 bytes). Takes the theme's resources out of the game's resource manager: the current theme's own resource (by the
// theme's name), then thirteen resources the game always has, by the names TPGlobals keeps ("SharedTex", "Small_SharedTex", "Rides",
// "Features", "Sideshow", "Upgrades", "Shops", "LocalAdvisor", "Terrain", "Dynamic", "Hoarding", "MiscMesh", "Queue"), in that order:
// FindResource, and when it answers 1, UnregisterResource of what it found. The Mac works on gGame (0x1037f134, through the TOC), not on
// `this`, for the theme's name and the manager; the manager pointer is read again before every call (the first read is the load-with-update
// of the address gGame + 0x34, the others plain loads through it), so a call that replaces it is honoured.
void CGame::ClearThemeResources() {
    CTPBaseResource* resource;
    const TbDynamicStringTemplate<char>& theme = gGame.GetCurrentThemeName();   // (before the manager is read)
    if (gGame.mResourceManager->FindResource(theme, &resource) == 1) {
        gGame.mResourceManager->UnregisterResource(resource);
    }
    const TbDynamicStringTemplate<char>* const names[] = {
        &gTPGlobals.mF4614, &gTPGlobals.mF4628, &gTPGlobals.mF4650, &gTPGlobals.mF4740, &gTPGlobals.mF472C,
        &gTPGlobals.mF4704, &gTPGlobals.mF4718, &gTPGlobals.mF47CC, &gTPGlobals.mF46C8, &gTPGlobals.mF47E0,
        &gTPGlobals.mF47F4, &gTPGlobals.mF4808, &gTPGlobals.mF481C,
    };
    for (const TbDynamicStringTemplate<char>* name : names) {
        if (gGame.mResourceManager->FindResource(*name, &resource) == 1) {
            gGame.mResourceManager->UnregisterResource(resource);
        }
    }
}

// Ghidra says void; the listing returns what the database answered.
int32_t CGame::LoadTheme(const char* path) { return gRideDatabase.LoadTheme(path); }

// The level goes, then the mouse object: the original's inline destructor calls AssignMouseMode(NULL) and the memory is freed
// (~CMouse here is that same inline call). The pointer is cleared whether or not there was a mouse.
void CGame::DeInit() {   // 0x1014965c
    ExitLevel();
    if (gMouse != nullptr) delete gMouse;
    gMouse = nullptr;
}

// CGame::ExitLevel (main app, SimTheme Park 2.04b3, Game.cpp; unit w75).

// 0x101496ec (292 bytes). The end of a level, in the Mac's order:
//  1. the world, when there is one (gGameWorld is read again before each step): CWorld::ExitLevel, CWorld::Free, then the block is
//     destroyed as a CStaffDB (a direct call of 0x10128f68 with the flag -1, which does not free the block) and given back with the
//     plain operator delete; gGameWorld is cleared even when the world's own functions replaced it;
//  2. the ride database is cleared and the sky closed;
//  3. with display flag 0x400000 and a sky object set, the sky objects are destroyed and deleted and the member cleared;
//  4. the message centre, when there is one, is destroyed (__destroy_arr over its 29 headers of 0x10 bytes, last first, the
//     destructor 0x101522e0 with the flag -1) and its block freed; the global is cleared even when it was NULL;
//  5. the theme's resources are cleared.
// The Mac answers 1 in r3 (the callers ignore it).
void CGame::ExitLevel() {
    if (gGameWorld != nullptr) {
        gGameWorld->ExitLevel();
        gGameWorld->Free();
        CWorld* world = gGameWorld;
        if (world != nullptr) {
            reinterpret_cast<CStaffDB*>(world)->~CStaffDB();
            ::operator delete(world);
        }
        gGameWorld = nullptr;
    }
    gRideDatabase.ClearDatabase();
    gei_Sky.Close();
    if ((gei_Display.mFlags & 0x400000) != 0 && mSkyObjects != nullptr) {
        mSkyObjects->DestroySkyObjects();
        delete mSkyObjects;
        mSkyObjects = nullptr;
    }
    // (CMessageCentre holds the 29 headers by value: its implicit destructor is __destroy_arr, last element first.)
    if (gMessageCentre != nullptr) delete gMessageCentre;
    gMessageCentre = nullptr;
    ClearThemeResources();
}

// CGame::Iterate (main app, 0x10149838, 168 bytes; unit w129).

// A function-local static of Iterate in the original (value at 0x1037f130, guard byte at 0x1037f12c): the first call stores 1 into it, but the store
// below overwrites that in the same call, so it is a plain global here (its initial 1 is the first-call store).
int32_t gGameIterateFlag = 1;

// Whether bit 3 (value 8) of gTPWFlags was set is read BEFORE the game clock steps a frame and kept in the static AFTER it; the flags are read again for
// the restart test (a restart asks the application to quit: bit 1 of gTPWFlags, value 2, is set).
void CGame::Iterate() {
    const uint32_t flags = gTPWFlags;
    const bool eightWasSet = (flags & 8) != 0;
    gClock.mTime.NextFrame();
    gGameIterateFlag = eightWasSet;
    if (mRestart != 0) gTPWFlags |= 2;
}

// Game.cpp's pause/resume routines (main app 0x10149904, 0x101499d0).

void CGame::Pause(ePauseFlag flag, bool allSounds) {
    if (mF3C != 1) return;
    mF1C = 1;
    if (static_cast<int32_t>(flag) == 1) gUIInterface.Pause();
    gClock.Pause();
    mF20 = static_cast<uint8_t>(allSounds);
    if (allSounds) {
        SOUND_Pause();
    } else {
        ADV_Pause();
        SOUND_SetPauseOn3DSoundsOnly(1);
    }
}

void CGame::Unpause() {
    if (mF3C != 1) return;
    mF1C = 0;
    gUIInterface.UnPause();
    gClock.UnPause();
    if (mF20 != 0) {
        SOUND_UnPause();
    } else {
        ADV_UnPause();
        SOUND_SetPauseOn3DSoundsOnly(0);
    }
}

// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).

// 0x10149a74 (212 bytes): only while mF3C is 1. The UI and the clock toggle; then a paused game resumes (the sounds as the last pause left
// them: all of them when mF20 is set, else the advisor and the 3D sounds) or a running one pauses (mF20 takes the argument).
void CGame::TogglePause(bool allSounds) {
    if (mF3C != 1) return;
    gUIInterface.TogglePause();
    gClock.TogglePause();
    if (mF1C == 1) {
        mF1C = 0;
        if (mF20 != 0) {
            SOUND_UnPause();
        } else {
            ADV_UnPause();
            SOUND_SetPauseOn3DSoundsOnly(0);
        }
    } else {
        mF1C = 1;
        mF20 = allSounds;
        if (mF20 != 0) {
            SOUND_Pause();
        } else {
            ADV_Pause();
            SOUND_SetPauseOn3DSoundsOnly(1);
        }
    }
}

int32_t CGame::GetCurrentTheme() const { return mCurrentTheme; }

// Game.cpp (main app 0x10149ba8..0x1014a44c): the setters of the current theme. The themes are the folder names in
// gGame.mThemes (FS_ScanThemes fills them); the current one is an index into that list and a copy of its name. Every setter ends
// in CFSGlobalMasterSave::ChangeCurrentTheme, so the master save always follows the game's theme.

namespace {

// The master save is made on first use; every setter carries this sequence inline. (A failed allocation would leave
// gMasterSave NULL, as the Mac's operator new answers NULL; nothing here tests for it.)
CFSGlobalMasterSave* TheMasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

}  // namespace

// Makes `theme` the current theme's index (the name follows only when the index changes) and tells the master save.
void CGame::SetCurrentTheme(int32_t theme) {
    if (mCurrentTheme != theme) {
        mCurrentTheme = theme;
        SetCurrentThemeName();
    }
    CFSGlobalMasterSave* save = TheMasterSave();
    save->ChangeCurrentTheme(GetCurrentThemeName());
}

// Finds the theme called `name` (ignoring case). Every theme is made current on the way, in the master save too (it is told each
// time), and the comparison is made on a COPY of the current name. A name that is not found puts the original theme back
// (and tells the master save once more).
void CGame::SetCurrentTheme(const TbDynamicStringTemplate<char>& name) {
    const int32_t original = mCurrentTheme;
    bool found = false;
    for (uint32_t index = 0; index < static_cast<uint32_t>(mThemes.mCount); ++index) {
        if (mCurrentTheme != static_cast<int32_t>(index)) {
            mCurrentTheme = static_cast<int32_t>(index);
            SetCurrentThemeName();
        }
        CFSGlobalMasterSave* save = TheMasterSave();
        save->ChangeCurrentTheme(GetCurrentThemeName());
        int32_t difference;
        {
            TbDynamicStringTemplate<char> copy(GetCurrentThemeName());
            difference = NS_MacDoze::_stricmp(copy.mData, name.mData);
        }
        if (difference == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (mCurrentTheme != original) {
            mCurrentTheme = original;
            SetCurrentThemeName();
        }
        CFSGlobalMasterSave* save = TheMasterSave();
        save->ChangeCurrentTheme(GetCurrentThemeName());
    }
}

// A copy of `name` goes into the list before the first theme it does not sort after.
// (The Find can never succeed: it looks for the copy just made.)
void CGame::AddThemeName(const TbDynamicStringTemplate<char>& name) {
    auto* copy = new TbDynamicStringTemplate<char>;
    *copy = name;
    if (mThemes.mCount == 0) {
        mThemes.AddHead(copy);
        return;
    }
    if (mThemes.Find(copy) != nullptr) return;
    POSITION position = mThemes.mHead;
    while (position != nullptr) {
        auto* node = static_cast<TbTList<TbDynamicStringTemplate<char>*, TbDynamicStringTemplate<char>*>::CNode*>(position);
        if (!TPStrGtr(*copy, *node->mData)) break;
        position = node->mNext;
    }
    if (position == nullptr) mThemes.AddTail(copy);
    else mThemes.InsertBefore(copy, position);
}

void CGame::ClearThemeNames() {
    while (mThemes.mCount != 0) {
        TbDynamicStringTemplate<char>* name = mThemes.RemoveHead();
        if (name != nullptr) delete name;
    }
}

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() { return mCurrentThemeName; }

// The theme folder names (built by __sinit_Game_cpp on the Mac, with the same text; the order of the comparisons below is the
// order of these addresses going down: fantasy, hallow, jungle, space).
TbDynamicStringTemplate<char16_t> gGameThemeFantasy(u"fantasy");   // 0x1037f10c
TbDynamicStringTemplate<char16_t> gGameThemeHallow(u"hallow");     // 0x1037f0ec
TbDynamicStringTemplate<char16_t> gGameThemeJungle(u"jungle");     // 0x1037f0cc
TbDynamicStringTemplate<char16_t> gGameThemeSpace(u"space");       // 0x1037f0ac

// The two gate signs of a theme. The theme's gate ride type (fantasy 4601, hallow 2601, jungle 1601, space 3601) gives the sign
// texts through the ride database into two buffers of 32 characters, which are NOT cleared first: a theme that is none of the
// four assigns whatever the buffers hold. The assignments go through the strings' virtual operator=(const Char*). The local
// narrow string is constructed and destroyed and never used (kept so that the calls run as on the Mac).
void CGame::GetLocalisedThemeGateNames(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& gate1,
                                       TbDynamicStringTemplate<char16_t>& gate2) const {
    TbDynamicStringTemplate<char> unused;
    char16_t second[0x20];
    char16_t first[0x20];
    if (TPUniStrEq(theme, gGameThemeFantasy)) {
        const uint16_t type = 0x11f9;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    } else if (TPUniStrEq(theme, gGameThemeHallow)) {
        const uint16_t type = 0xa29;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    } else if (TPUniStrEq(theme, gGameThemeJungle)) {
        const uint16_t type = 0x641;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    } else if (TPUniStrEq(theme, gGameThemeSpace)) {
        const uint16_t type = 0xe11;
        gRideDatabase.GetSignNameA(type, first, 0x20);
        gRideDatabase.GetSignNameB(type, second, 0x20);
    }
    gate1 = static_cast<const char16_t*>(first);
    gate2 = static_cast<const char16_t*>(second);
}

// Copies the name of theme number mCurrentTheme into mCurrentThemeName (the string's virtual assignment, slot 0x1c). The walk
// has no end test: an index past the list, or any index of an empty list, reads through a NULL node (a negative index takes the
// first name).
void CGame::SetCurrentThemeName() {
    auto* node = mThemes.mHead;
    for (int32_t i = 0; i < mCurrentTheme; ++i) node = node->mNext;
    mCurrentThemeName = *node->mData;
}

// The old theme numbers of the settings file (CURRENTTheme) to the folder order: 3 is the first folder, 1 the third, 2 the second,
// 4 the fourth, anything else (0, negatives, 5 and more) the third. The Mac has the name copy and the master save inline.
void ConverttoCurrentTheme(int32_t theme) {
    int32_t index;
    switch (theme) {
    case 3: index = 0; break;
    case 1: index = 2; break;
    case 2: index = 1; break;
    case 4: index = 3; break;
    default: index = 2; break;
    }
    if (gGame.mCurrentTheme != index) {
        gGame.mCurrentTheme = index;
        gGame.SetCurrentThemeName();
    }
    CFSGlobalMasterSave* save = TheMasterSave();
    save->ChangeCurrentTheme(gGame.mCurrentThemeName);
}

// 0x1014a478 (136 bytes): the theme's folder name as a narrow string: converted to a wide one, which the wide overload turns into the player's
// language name when the conversion answered nonzero.
void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    TbDynamicStringTemplate<char16_t> converted;
    if (StringToUniString(converted, theme) != 0) GetLocalisedThemeName(converted, out);
}

// CGame::GetLocalisedThemeName (0x1014a570): the name of a theme in the player's language.

void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[22]);   // gText + 0x5a4
    TbDynamicStringTemplate<char> unused;   // built and destroyed, never used
    if (TPUniStrEq(theme, gGameThemeFantasy)) {
        out = UIMain_ConvertMulti(table->GetConstString(2));
    } else if (TPUniStrEq(theme, gGameThemeHallow)) {
        out = UIMain_ConvertMulti(table->GetConstString(1));
    } else if (TPUniStrEq(theme, gGameThemeJungle)) {
        out = UIMain_ConvertMulti(table->GetConstString(0));
    } else if (TPUniStrEq(theme, gGameThemeSpace)) {
        out = UIMain_ConvertMulti(table->GetConstString(3));
    }
}
