// TPConfig.cpp: name proven by __sinit_TPConfig_cpp in the main application.

#include "Game.h"
#include "Quickload.h"
#include "ResolutionBalance.h"
#include "rsse.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include <cstring>


// TPConfig.cpp (decompiled from the main app of SimTheme Park 2.04b3: 0x100591a4..0x1005999c
// and __sinit_TPConfig_cpp at 0x100599c8): the start-up stages ThemeParkWorld::Process runs,
// the search path and the data file lookup.
//
// The inventory files coaster and layout code (0x10040...-0x10058...) under this name too;
// it is untranslated.

char gFilePathBuffer[0x400];   // 0x102c1798
char* gPathString;             // 0x102c1b98
char** gPaths;                 // 0x102c1b9c
int32_t gPathLength;           // 0x102c1ba0

// __sinit_TPConfig_cpp, in construction order: the same strings as TPMain.cpp's
// gTPMainLanguages (each file has its own copies). Nothing in this file reads them.
TbDynamicStringTemplate<char> gTPConfigLanguages[16] = {   // 0x102c1be4, 20-byte slots
    TbDynamicStringTemplate<char>("English"),
    TbDynamicStringTemplate<char>("French"),
    TbDynamicStringTemplate<char>("German"),
    TbDynamicStringTemplate<char>("Spanish"),
    TbDynamicStringTemplate<char>("Swedish"),
    TbDynamicStringTemplate<char>("Italian"),
    TbDynamicStringTemplate<char>("Dutch"),
    TbDynamicStringTemplate<char>("Polish"),
    TbDynamicStringTemplate<char>("Danish"),
    TbDynamicStringTemplate<char>("Japanese"),
    TbDynamicStringTemplate<char>("TraditionalChinese"),
    TbDynamicStringTemplate<char>("SimplifiedChinese"),
    TbDynamicStringTemplate<char>("Thai"),
    TbDynamicStringTemplate<char>("Korean"),
    TbDynamicStringTemplate<char>("Hebrew"),
    TbDynamicStringTemplate<char>("American")
};
TbDynamicStringTemplate<char> gTPConfigTitle("Theme Park World");    // 0x102c1bc4
TbDynamicStringTemplate<char> gTPConfigLanguage("Language");         // 0x102c1ba4

namespace {

// The search paths FindFilePath tries (gPaths has this many entries).
constexpr int32_t kSearchPaths = 1;

void SetScreenSize(int32_t width, int32_t height) {
    gScreenWidth = width;
    gScreenHeight = height;
}


}  // namespace

void InitialiseGameStage1() {
    gTPWFlags = 0;
    // Full screen, front end, sound, 640 x 480 and the bits 0x4, 0x800, 0x40000, 0x80000.
    gGameOptions = 0xc0e15;
    if (gPlayerSettings.mCustomOptions != 0) {
        gGameOptions = gPlayerSettings.mGameOptions;
        gTPDialogOptions = gPlayerSettings.mExtraEngineOptions;
        ConverttoCurrentTheme(gPlayerSettings.mCurrentTheme);
        gTPDialogTextureMegabytes = gPlayerSettings.mTextureCacheSize;
    }
}

void InitialiseGameStage2() { BuildGeometryTables(0x1000); }

void InitialiseGameStage3() {
    gPathLength = gTPGlobals.mSearchPath.Length() + 1;
    gPaths = static_cast<char**>(MReleaseAllocF(kSearchPaths * sizeof(char*), 1));
    gPathString = static_cast<char*>(MReleaseAllocF(static_cast<uint32_t>(gPathLength), 1));
    gPaths[0] = gPathString;
    std::strcpy(gPaths[0], gTPGlobals.mSearchPath.mData);

    if (gPlayerSettings.mCustomOptions == 1) {
        // The options dialog's screen size bit; 160 x 120 only in a window.
        const uint32_t options = gGameOptions;
        if ((options & 0x20000000) != 0 && (options & kGameOptionFullScreen) == 0) SetScreenSize(160, 120);
        else if ((options & 0x40000000) != 0) SetScreenSize(320, 240);
        else if ((options & 0x80000000) != 0) SetScreenSize(400, 300);
        else if ((options & 0x8) != 0) SetScreenSize(512, 384);
        else if ((options & 0x10) != 0) SetScreenSize(640, 480);
        else if ((options & 0x20) != 0) SetScreenSize(800, 600);
        else if ((options & 0x40) != 0) SetScreenSize(1024, 768);
        else if ((options & 0x80) != 0) SetScreenSize(1280, 1024);
        else if ((options & 0x100) != 0) SetScreenSize(1600, 1200);
        else SetScreenSize(640, 480);
    } else if (gResolutionBalance.mValues[1].mInteger > 0) {
        // Resolution.sam's choice; other values keep the current size.
        switch (gResolutionBalance.mValues[1].mInteger) {
        case 1: SetScreenSize(512, 384); break;
        case 2: SetScreenSize(640, 480); break;
        case 3: SetScreenSize(800, 600); break;
        case 4: SetScreenSize(1024, 768); break;
        case 5: SetScreenSize(1280, 1024); break;
        case 6: SetScreenSize(1600, 1200); break;
        case 7: SetScreenSize(2048, 1536); break;
        default: break;
        }
    } else {
        switch (gPlayerSettings.mCurrent.mResolution) {
        case 0: SetScreenSize(512, 384); break;
        case 1: SetScreenSize(640, 480); break;
        case 2: SetScreenSize(800, 600); break;
        case 3: SetScreenSize(1024, 768); break;
        default: SetScreenSize(640, 480); break;
        }
    }

    // The sizes convert as unsigned.
    gViewCentreX = static_cast<float>(static_cast<uint32_t>(gScreenWidth)) * 0.5f;
    gViewCentreY = static_cast<float>(static_cast<uint32_t>(gScreenHeight)) * 0.5f;
    gScreenCentreX = gViewCentreX;
    gScreenCentreY = gViewCentreY;
    gTPWFlags |= 8;
    ResetWorldBoundingArea();
    InitialiseQuickloading();
}

void ClosedownGame() {
    BuildGeometryTables(0x1000);
    CloseQuickloading();
    if (gPathString != nullptr) {
        MReleaseFreeF(gPathString);
        gPathString = nullptr;
    }
    if (gPaths != nullptr) {
        MReleaseFreeF(gPaths);
        gPaths = nullptr;
    }
}

int32_t DoesFileExist(char* name, uint32_t flags) {
    int32_t found = 0;
    char* separator = std::strrchr(name, ':');
    char* base = separator != nullptr ? separator + 1 : name;
    if ((flags & kFileQuickload) != 0) {
        found = DoesQuickloadFilenameExist(base, flags & kFileTextures, GetFileAccessIndex());
        if ((flags & kFileFallBackToDisk) == 0) return found;
    }
    if (found == 0) found = CheckFileExists(name);
    return found;
}

char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    if (name == nullptr) return nullptr;
    if (gQuickloadEnabled == 0) flags &= ~kFileQuickload;
    int32_t indexed = -1;
    if ((flags & kFileQuickload) != 0) {
        const char* separator = std::strrchr(name, ':');
        indexed = FindQuickloadFilename(separator != nullptr ? separator + 1 : name, flags & kFileTextures);
        if (result != nullptr) *result = indexed;
        if (indexed == -1) {
            if ((flags & kFileFallBackToDisk) == 0) return nullptr;
            flags &= ~kFileQuickload;
        }
    } else if (result != nullptr) {
        *result = -1;
    }
    for (int32_t i = 0; i < kSearchPaths; ++i) {
        const uint32_t quick = flags & kFileQuickload;
        if (quick != 0) i = indexed;
        char* path = gPaths[i];
        std::strcpy(gFilePathBuffer, name);
        if (quick != 0) return (flags & kFileReturnName) != 0 ? gFilePathBuffer : path;
        if (CheckFileExists(gFilePathBuffer) != 0) {
            if (result != nullptr) *result = i;
            return (flags & kFileReturnName) != 0 ? gFilePathBuffer : path;
        }
    }
    if (result != nullptr) *result = -1;
    return nullptr;
}

void TPSaveRegistry() {}
