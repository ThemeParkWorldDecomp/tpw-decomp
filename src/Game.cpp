// Game.cpp range of the main app (SimTheme Park 2.04b3): the game object's construction,
// resources and theme names. (The command line, from the same range, is in
// CommandLine.cpp.) PARTIAL: the levels, themes, pausing and the rest of CGame are still
// to do.
#include "Game.h"

#include "FS_System.h"
#include "Mouse.h"
#include "SkyObjects.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"

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

// The level goes, then the mouse object: the original's inline destructor calls AssignMouseMode(NULL) and the memory is freed
// (~CMouse here is that same inline call). The pointer is cleared whether or not there was a mouse.
void CGame::DeInit() {   // 0x1014965c
    ExitLevel();
    if (gMouse != nullptr) delete gMouse;
    gMouse = nullptr;
}
