// The quickload indexes (decompiled from the main app of SimTheme Park 2.04b3:
// 0x1006736c..0x10068248). The inventory files this range under TPRide.cpp; the original
// file name is unknown (see Quickload.h).
//
// A qickload.txt (sic) lists the files of its folder, separated by white space. The game
// loads the list of the current mesh folder and of its theme texture folder for each search
// path, and DoesFileExist / FindFilePath (TPConfig.cpp) consult them before the disk.
#include "Quickload.h"

#include <cstdio>
#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "TPAdvisor.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "ui_ErrorDisplay.h"

sQuickloadIndex* gQuickloadTextureIndexes;   // 0x102c7994
sQuickloadIndex* gQuickloadIndexes;          // 0x102c7998
int32_t gQuickloadInitialised;               // 0x102c799c
int32_t gQuickloadEnabled;                   // 0x102c79a0
int32_t gQuickloadPathCount = 1;             // 0x1029cf08

namespace {

sQuickloadIndex* IndexOf(int32_t path, bool textures) {
    return textures ? &gQuickloadTextureIndexes[path] : &gQuickloadIndexes[path];
}

// A whole file in a new block, its size in *size; NULL when it does not open (*size
// untouched) or the block cannot be allocated. Inlined in LoadQuickloadIndexes.
void* LoadWholeFile(const char* name, uint32_t* size) {
    bfl::FileSystem::CFile* file = TPFile_Open(name, "r");
    if (file == nullptr) return nullptr;
    const uint32_t length = static_cast<uint32_t>(TPFile_Size(file));
    void* data = MReleaseAllocF(length, 1);
    if (data != nullptr) TPFile_Read(data, length, 1, file);
    TPFile_Close(file);
    *size = length;
    return data;
}

// Whether the '.' at `dot` ends a folder name (a ':' follows it) rather than starting an
// extension. Inlined, with StripExtension, twice in CheckIndexedFileExists.
bool DotEndsFolder(const char* dot) { return dot != nullptr && dot + 1 != nullptr && dot[1] == ':'; }

void StripExtension(char* name) {
    char* dot = std::strrchr(name, '.');
    if (dot != nullptr && !DotEndsFolder(dot)) *dot = '\0';
}

}  // namespace

void InitialiseQuickloading() {
    gQuickloadInitialised = 1;
    gQuickloadIndexes = static_cast<sQuickloadIndex*>(
        MReleaseAllocF(static_cast<uint32_t>(gQuickloadPathCount * sizeof(sQuickloadIndex)), 1));
    gQuickloadTextureIndexes = static_cast<sQuickloadIndex*>(
        MReleaseAllocF(static_cast<uint32_t>(gQuickloadPathCount * sizeof(sQuickloadIndex)), 1));
}

// Two passes over the text: the first counts the names and their bytes, the second copies
// them into one block.
void LoadQuickloadIndex(sQuickloadIndex* index, void* data, uint32_t size) {
    uint32_t count = 0;
    uint32_t bytes = 0;
    char* text = nullptr;
    char name[0x100];
    sFileParse parse = {};
    for (int32_t pass = 0; pass < 2; ++pass) {
        parse.mCursor = static_cast<char*>(data);
        parse.mSize = size;
        parse.mPos = 0;
        const bool store = pass != 0;
        if (store) {
            index->mText = static_cast<char*>(MReleaseAllocF(bytes, 1));
            index->mNames = static_cast<char**>(MReleaseAllocF(static_cast<uint32_t>(count * sizeof(char*)), 1));
            index->mCount = count;
            count = 0;
            text = index->mText;
        }
        for (;;) {
            if (SkipWhitespace(&parse) != 0) break;
            if (IsWhiteSpace(&parse) != 0) continue;
            const int32_t end = GetString(&parse, name, sizeof(name), 1, 1, 1, 1);
            if (store) {
                if (name[0] != '\0') {
                    std::strcpy(text, name);
                    index->mNames[count] = text;
                    text += std::strlen(name) + 1;
                }
            } else if (name[0] != '\0') {
                bytes += static_cast<uint32_t>(std::strlen(name) + 1);
            }
            if (name[0] != '\0') ++count;
            if (end != 0) break;
        }
    }
}

void LoadQuickloadIndexes(const char* folder, int32_t path) {
    if (gQuickloadInitialised == 0) return;
    for (int32_t k = 0; k < 2; ++k) {
        const bool textures = k != 0;
        char name[0x400];
        if (textures)
            std::snprintf(name, sizeof(name), "%s:%s:%s:qickload.txt", gPaths[path], folder, gThemeTextureFolder);
        else
            std::snprintf(name, sizeof(name), "%s:%s:qickload.txt", gPaths[path], folder);
        sQuickloadIndex* index = IndexOf(path, textures);
        uint32_t size = 0;
        void* data = LoadWholeFile(name, &size);
        if (index != nullptr) std::memset(index, 0, sizeof(*index));
        if (data != nullptr) {
            if (k == 1 && gQuickloadEnabled == 0)
                CTRACE(5, "WARNING!  Quickloading currently requires qickload.txt files in both or none "
                          "directories - this ride will likely not get animations\n");
            gQuickloadEnabled = 1;
            LoadQuickloadIndex(index, data, size);
            MReleaseFreeF(data);
        }
    }
}

void FreeQuickloadIndexes() {
    if (gQuickloadEnabled != 0) {
        for (int32_t path = gQuickloadPathCount - 1; path >= 0; --path) {
            for (int32_t k = 0; k < 2; ++k) {
                sQuickloadIndex* index = IndexOf(path, k == 0);   // the texture index first
                MReleaseFreeF(index->mNames);
                index->mNames = nullptr;
                MReleaseFreeF(index->mText);
                index->mText = nullptr;
            }
        }
    }
    gQuickloadEnabled = 0;
}

void CloseQuickloading() {
    MReleaseFreeF(gQuickloadIndexes);
    gQuickloadIndexes = nullptr;
    MReleaseFreeF(gQuickloadTextureIndexes);
    gQuickloadTextureIndexes = nullptr;
    gQuickloadEnabled = 0;
    gQuickloadInitialised = 0;
}

int32_t DoesQuickloadFilenameExist(const char* name, int32_t textures, int32_t path) {
    if (gQuickloadEnabled == 0) return -1;
    const sQuickloadIndex* index = IndexOf(path, textures != 0);
    for (uint32_t i = 0; i < index->mCount; ++i)
        if (NS_MacDoze::_stricmp(name, index->mNames[i]) == 0) return 1;
    return 0;
}

int32_t FindQuickloadFilename(const char* name, int32_t textures) {
    if (gQuickloadEnabled == 0) return -1;
    for (int32_t path = 0; path < gQuickloadPathCount; ++path) {
        const sQuickloadIndex* index = IndexOf(path, textures != 0);
        for (uint32_t i = 0; i < index->mCount; ++i)
            if (NS_MacDoze::_stricmp(name, index->mNames[i]) == 0) return path;
    }
    return -1;
}

int32_t CheckIndexedFileExists(const char* name, int32_t path, int32_t textures) {
    char wanted[0x400];
    char candidate[0x400];
    std::strcpy(wanted, name);
    StripExtension(wanted);
    if (gQuickloadEnabled == 0) return 0;
    const sQuickloadIndex* index = IndexOf(path, textures != 0);
    for (uint32_t i = 0; i < index->mCount; ++i) {
        std::strcpy(candidate, index->mNames[i]);
        StripExtension(candidate);
        if (NS_MacDoze::_stricmp(wanted, candidate) == 0) return 1;
    }
    return 0;
}

int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder) {
    char path[0x400];
    char textureFolder[0x400];
    int32_t index;
    FreeQuickloadIndexes();
    // The original passes `file` alone as sprintf's format; file names have no '%'.
    if (folder != nullptr) std::snprintf(path, sizeof(path), "%s:%s", folder, file);
    else std::snprintf(path, sizeof(path), "%s", file);
    char* root = FindFilePath(path, 0, &index);
    if (index != -1) {
        LoadQuickloadIndexes(folder, index);
        SetFileAccessIndex(index);
        std::strcpy(textureFolder, folder);
        const std::size_t length = std::strlen(textureFolder);
        char* end = textureFolder + length;
        if (end[-1] == ':') std::snprintf(end, sizeof(textureFolder) - length, "%s", gThemeTextureFolder);
        else std::snprintf(end, sizeof(textureFolder) - length, ":%s", gThemeTextureFolder);
        if (root != nullptr) {
            SetNewMeshPaths(root, folder, textureFolder, sharedTextureFolder);
            return 1;
        }
    }
    CTRACE(5, "Could not find ride '%s' - check registry paths and quickload indexes\n", path);
    return 0;
}
