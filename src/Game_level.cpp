// CGame::InitTheme and the level it loads (main app, SimTheme Park 2.04b3, Game.cpp; unit w117): InitTheme 0x10147ee4, InitLevel 0x10148028,
// InitLevel_Load 0x1014809c, InitLevel_Initialise 0x101482bc and LoadTheme 0x1014960c.
#include "Game.h"

#include <cstdint>

#include "Advisor.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_System.h"
#include "HLSprite.h"
#include "HoardingsBalance.h"
#include "LLCellSystem.h"
#include "LevelInit.h"
#include "Main.h"
#include "MainBalance.h"
#include "SkyObjects.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "tb_resource_display.h"
#include "tb_sky.h"

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

// Ghidra says void; the listing returns what the database answered.
int32_t CGame::LoadTheme(const char* path) { return gRideDatabase.LoadTheme(path); }

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
