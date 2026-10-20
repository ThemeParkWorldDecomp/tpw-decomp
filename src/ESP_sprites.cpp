// ESP_sprites.cpp: the ESP sprite loader (HLSprite.cpp of the main app, SimTheme Park 2.04b3): ESP_CSprite's
// constructor, GetActionFrames, GetCustomData and LoadFromESPFile (0x1007b150..0x1007b9e8), ESP_SortFileList
// (0x1007ba5c), ESP_FindESPFiles (0x1007bb80), ESP_LoadSprites (0x1007c63c), ESP_GetParticleSprite (0x1007cefc) and
// ESP_InitialiseAndLoadSprites (0x1007ace0, declared in HLSprite.h).
//
// THE .ESP FILE (little-endian, 334 bytes; the shipped files have 350, with the custom blocks):
//   0x000  8 bytes "ESP_FILE"        read and dropped
//   0x008  4 bytes "2.00"            read and dropped
//   0x00c  0x100 bytes               the data file's name ("SPR_BE.TPS", zero padded), read and dropped: the loader makes
//                                    the data file's name from the .ESP file's own
//   0x10c  1 byte                    mHasFirstPerson: the set also has first person sprites
//   0x10d  1 byte                    mF206
//   0x10e  16 actions of 4 bytes     u16 first record, u8 records in a view, u8 views flag
//   0x14e  16 bytes (optional)       four custom blocks of four bytes; the files before them end at 0x14e (the read of
//                                    the first byte answers 0 there and the other 15 are not read)
// The data file ("SPR_BE.TPC": the .ESP file's name with the extension replaced) is read by CSpriteFileIO::Load.
#include "ESP_sprites.h"

#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "HLSprite.h"
#include "MapWho.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPOptions.h"
#include "TPString.h"
#include "m3d2.h"
#include "tb_resource_display.h"

using bfl::FileSystem::CFile;
using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

// ---- data
ESPFileEntry gESPFiles[kESPMaxFiles];
int32_t gESPFileCount;
uint8_t gESPSemitransparent;
int32_t gESPSpriteCount;
int32_t gESPTotalSprites;
char gESPPathBuffer[0x200];
CFileInfo gESPFileInfo;
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
ESPSpriteGroup gESPGroups[kESPGroups] = {
    {"kids", 0, 0},         {"kidsheads", 0, 0},   {"costumes", 0, 0},  {"costumeheads", 0, 0}, {"entertainers", 0, 0},
    {"handymen", 0, 0},     {"mechanics", 0, 0},   {"guards", 0, 0},    {"researchers", 0, 0},  {"thoughts", 0, 0},
    {"balloons", 0, 0},     {"litter", 0, 0},      {"specialfx", 0, 0}, {"particles", 0, 0},
};
const char* gESPKidFiles[4] = {"SPR_BI.ESP", "SPR_KI.ESP", "SPR_TA.ESP", "SPR_SU.ESP"};
uint8_t gESPCompressed = 1;
int32_t gESPFirstPerson;
int32_t gESPThirdPerson = 1;
uint8_t gESPDrawEnabled = 1;
int32_t gESPCellsHigh;
int32_t gESPCellsWide;
int32_t gESPMapHeight;
int32_t gESPMapWidth;
int32_t gESPF34;
CThing* gESPF20;
float gESPF88;
SSpriteGameData** gESPListInstances[4][4];
SSpriteGameData** gESPGlobalInstances;
CSpriteList gESPGlobalList;
float gSpriteHotX;
float gSpriteHotY;

// ---- ESP_CSprite
namespace {

// Reads a little-endian 16-bit field of the file. The Mac reads the two bytes into the field, which is big-endian there,
// and swaps it in place (sthbrx), so a read that comes short, or fails, swaps what the field held (a fresh sprite's 0
// stays 0). Here the field's bytes are put in the Mac's order for the read and swapped by hand.
void ReadLittleEndian16(CFile& file, uint16_t& field) {
    uint8_t memory[2] = {static_cast<uint8_t>(field >> 8), static_cast<uint8_t>(field)};
    file.Read(memory, 2);
    field = static_cast<uint16_t>(memory[0] | (memory[1] << 8));
}

}  // namespace

// Sets what the original sets and leaves the rest as the heap had it (see the class).
ESP_CSprite::ESP_CSprite() {
    mLoaded = 0;
    mFile = nullptr;
    mRecordCount = 0;
    mCustomCount = 0;
    mActionCount = 0;
    mSemitransparent = 1;
    for (int32_t block = 0; block < 4; ++block) {
        mCustom[block * 4 + 0] = 0;
        mCustom[block * 4 + 2] = 0;
        mCustom[block * 4 + 3] = 0;
    }
    for (int32_t i = 0; i < 16; ++i) {
        mActions[i].mFirstFrame = 0;
        mActions[i].mFrames = 0;
        mActions[i].mViews = 0;
    }
}

uint8_t ESP_CSprite::GetActionFrames(int32_t action) const { return mActions[action].mFrames; }

void ESP_CSprite::GetCustomData(int32_t block, int32_t* first, ACTS* second, int32_t* third, int32_t* fourth) const {
    if (block < 0 || block > 3) return;
    const uint8_t* bytes = &mCustom[block * 4];
    *second = static_cast<ACTS>(static_cast<int32_t>(bytes[1]) - 1);
    *first = static_cast<int32_t>(bytes[0]) - 1;
    *third = bytes[2];
    *fourth = bytes[3];
}

bool ESP_CSprite::LoadFromESPFile(CFileInfo& info, CFileStorage& storage) {
    // The name is the entry's buffer (the entry's mName, which can point into it, only goes to the trace).
    TbDynamicStringTemplate<char> path(info.mNameBuffer);
    CFile* file = new CFile;
    if (file == nullptr) return false;   // (the original's test of operator new; never taken)
    CTRACE(6, "Found file %s\n", info.mName);
    const TbError opened = storage.OpenFile(*file, info.mNameBuffer, 0x80000001, nullptr);
    if (!opened.Succeeded()) return false;

    // The header: the signature, the version and the data file's name are read and dropped.
    uint8_t header[0x100];
    file->Read(header, 8);
    file->Read(header, 4);
    file->Read(header, 0x100);
    file->Read(&mHasFirstPerson, 1);
    file->Read(&mF206, 1);

    // The name without its extension (the length of ".tps", all the extensions are four long).
    const int32_t nameLength = path.Length();
    const int32_t extensionLength = gTPGlobals.mF42E0.Length();
    const TbDynamicStringTemplate<char> stem(path.Left(static_cast<uint32_t>(nameLength - extensionLength)));
    std::strcpy(mName, stem.mData);

    // The actions.
    mActionCount = 0;
    for (int32_t i = 0; i < 16; ++i) {
        Action& action = mActions[i];
        ReadLittleEndian16(*file, action.mFirstFrame);
        file->Read(&action.mFrames, 1);
        file->Read(&action.mViews, 1);
        if (action.mFrames != 0) ++mActionCount;
    }

    // The custom blocks, when the file has them: the answer of the read of the first byte decides, and any answer but 0
    // (a failed read gives -1) goes on to read the other fifteen.
    for (int32_t block = 0; block < 4; ++block) mCustom[block * 4] = 0;
    mCustomCount = 0;
    if (file->Read(&mCustom[0], 1) != 0) {
        for (int32_t i = 1; i < 16; ++i) file->Read(&mCustom[i], 1);
    }
    for (int32_t block = 0; block < 4; ++block) {
        if (mCustom[block * 4] != 0) ++mCustomCount;
    }
    file->Close();

    // The data file: the same name with the extension of the sprite kind. The first person kind needs a first person
    // game and a set that has one; gESPCompressed picks the "c" extensions over the "s" ones.
    const int32_t length = path.Length();
    const int32_t shortLength = gTPGlobals.mF42E0.Length();
    path = path.Left(static_cast<uint32_t>(length - shortLength));
    const bool firstPerson = gESPFirstPerson != 0 && mHasFirstPerson != 0;
    if (gESPCompressed != 0) {
        if (firstPerson) path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF431C);      // ".fpc"
        else path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF42F4);                  // ".tpc"
    } else {
        if (firstPerson) path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4308);      // ".fps"
        else path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF42E0);                  // ".tps"
    }
    const TbError reopened = storage.OpenFile(*file, path.mData, 0x80000001, nullptr);
    if (!reopened.Succeeded()) return false;

    // The file object and the sprite file are never freed here (ESP_RemoveSprites frees the latter). A failed Load still
    // counts: mLoaded stays 0 and the answer is 1.
    mFile = new CSpriteFileIO;
    const int32_t loaded = CSpriteFileIO::Load(mFile, file);   // (the caller also clears r5, which Load never reads)
    mLoaded = loaded == 1 ? 1 : 0;
    mRecordCount = static_cast<uint32_t>(mFile->mCount);
    mSemitransparent = gESPSemitransparent;
    if (mSemitransparent != 0) mFile->FlagAsSemitransparent();
    ++gESPSpriteCount;
    return true;
}

// ---- the file list
void ESP_SortFileList() {
    int32_t swapped = 1;
    while (swapped != 0) {
        swapped = 0;
        for (int32_t i = 1; i < gESPFileCount; ++i) {
            ESPFileEntry& previous = gESPFiles[i - 1];
            ESPFileEntry& current = gESPFiles[i];
            const TbDynamicStringTemplate<char> previousName(previous.mInfo->mName);
            const TbDynamicStringTemplate<char> currentName(current.mInfo->mName);
            if (!TPStrGtr(previousName, currentName)) continue;
            const ESPFileEntry held = current;
            ++swapped;
            current = previous;
            previous = held;
        }
    }
}

namespace {

// Adds the entries FindFirst and FindNext give for `pattern` to gESPFiles, copies of the directory entry and of the
// storage it was found in; once the list is full the search goes on and keeps nothing.
void ListFiles(CTPSpriteResource& resource, const TbDynamicStringTemplate<char>& pattern) {
    if (resource.FindFirst(pattern, gESPFileInfo) != 1) return;
    do {
        CFileStorage* found = resource.pGetCurrentSearchStorage();
        if (gESPFileCount < kESPMaxFiles) {
            ESPFileEntry& entry = gESPFiles[gESPFileCount];
            entry.mInfo = new CFileInfo;
            entry.mStorage = new CFileStorage;
            *entry.mInfo = gESPFileInfo;
            *entry.mStorage = *found;
            ++gESPFileCount;
        }
    } while (resource.FindNext(gESPFileInfo) != 0);
}

// Frees the list: the entries and the copies of the storages (the entry pointers are cleared, the storage pointers are not).
void FreeFileList() {
    for (int32_t i = 0; i < gESPFileCount; ++i) {
        ESPFileEntry& entry = gESPFiles[i];
        if (entry.mInfo != nullptr) delete entry.mInfo;
        if (entry.mStorage != nullptr) delete entry.mStorage;
        entry.mInfo = nullptr;
    }
}

// Makes a sprite for the entry at the next free place of gESPSprites and loads it; a sprite that does not load is deleted
// (its place in the table keeps the pointer).
bool LoadFile(ESPFileEntry& entry) {
    ESP_CSprite* sprite = new ESP_CSprite;
    gESPSprites[gESPSpriteCount] = sprite;
    if (!sprite->LoadFromESPFile(*entry.mInfo, *entry.mStorage)) {
        if (gESPSprites[gESPSpriteCount] != nullptr) delete gESPSprites[gESPSpriteCount];
        return false;
    }
    return true;
}

}  // namespace

int32_t ESP_FindESPFiles(CTPSpriteResource& resource, const TbDynamicStringTemplate<char>& name) {
    TbDynamicStringTemplate<char> pattern;
    gESPSemitransparent = 1;
    int32_t made = 0;
    int32_t kidLimit = 0;
    int32_t staffLimit = 0;
    bool kids = false;
    bool staff = false;

    // Kids stop at the player's number of kid sprites, staff at the number of staff sprites.
    if (TPStrEq(name, TbDynamicStringTemplate<char>("kids")) || TPStrEq(name, TbDynamicStringTemplate<char>("kidsheads"))) {
        kids = true;
        kidLimit = gOptions.GetNumKidSprites();
    }
    if (TPStrEq(name, TbDynamicStringTemplate<char>("handymen")) ||
        TPStrEq(name, TbDynamicStringTemplate<char>("mechanics")) ||
        TPStrEq(name, TbDynamicStringTemplate<char>("guards")) ||
        TPStrEq(name, TbDynamicStringTemplate<char>("researchers"))) {
        staff = true;
        staffLimit = gOptions.GetNumStaffSprites();
    }

    gESPFileCount = 0;
    for (int32_t i = 0; i < kESPMaxFiles; ++i) {
        gESPFiles[i].mInfo = nullptr;
        gESPFiles[i].mStorage = nullptr;
        gESPFiles[i].mLoaded = 0;
    }

    // The generic folder, then the current theme's.
    std::strcpy(gESPPathBuffer, "generic:");
    std::strcat(gESPPathBuffer, name.mData);
    std::strcat(gESPPathBuffer, ":");
    pattern = "generic";
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(name);
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4588);
    ListFiles(resource, pattern);

    std::strcpy(gESPPathBuffer, gGame.GetCurrentThemeName().mData);
    std::strcat(gESPPathBuffer, ":");
    std::strcat(gESPPathBuffer, name.mData);
    std::strcat(gESPPathBuffer, ":");
    pattern = gGame.GetCurrentThemeName();
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(name);
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4588);
    ListFiles(resource, pattern);

    ESP_SortFileList();

    // The kids' four preferred files go first, in that order, so that a small number of kid sprites gets those.
    if (kids) {
        CTRACE(6, "Loading Avatar Sprites from %s\n", name.mData);
        for (int32_t preferred = 0; preferred < 4; ++preferred) {
            for (int32_t i = 0; i < gESPFileCount; ++i) {
                ESPFileEntry& entry = gESPFiles[i];
                bool wanted;
                {
                    const TbDynamicStringTemplate<char> fileName(entry.mInfo->mName);
                    const TbDynamicStringTemplate<char> preferredName(gESPKidFiles[preferred]);
                    wanted = TPStrEq(fileName, preferredName);
                }
                if (!wanted || entry.mLoaded != 0) continue;
                if (!LoadFile(entry)) continue;
                ++made;
                entry.mLoaded = 1;
                if (gESPSpriteCount > kESPMaxSprites - 1 || made >= kidLimit) {
                    FreeFileList();
                    return made;
                }
            }
        }
    }

    // Then every file not loaded yet, in name order.
    CTRACE(6, "Loading Sprites from %s\n", name.mData);
    for (int32_t i = 0; i < gESPFileCount; ++i) {
        ESPFileEntry& entry = gESPFiles[i];
        if (entry.mLoaded != 0) continue;
        if (!LoadFile(entry)) continue;
        ++made;
        entry.mLoaded = 1;
        if (gESPSpriteCount > kESPMaxSprites - 1 || (kids && made >= kidLimit) || (staff && made >= staffLimit)) {
            FreeFileList();
            return made;
        }
    }
    FreeFileList();
    return made;
}

bool ESP_LoadSprites(bool particlesOnly) {
    CTPBaseResource* found = nullptr;
    gESPSpriteCount = 0;
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4664, &found) != 1) return false;
    CTPSpriteResource& resource = static_cast<CTPSpriteResource&>(*found);
    if (particlesOnly) {
        ESPSpriteGroup& group = gESPGroups[kESPGroups - 1];
        group.mFirst = gESPSpriteCount;
        const TbDynamicStringTemplate<char> folder(group.mName);
        group.mCount = ESP_FindESPFiles(resource, folder);
    } else {
        for (int32_t i = 0; i < kESPGroups; ++i) {
            ESPSpriteGroup& group = gESPGroups[i];
            group.mFirst = gESPSpriteCount;
            const TbDynamicStringTemplate<char> folder(group.mName);
            group.mCount = ESP_FindESPFiles(resource, folder);
        }
    }
    gESPTotalSprites = gESPSpriteCount;
    return true;
}

PTCLTexture* ESP_GetParticleSprite(int32_t sprite, int32_t frame, int32_t variant) {
    // Division and remainder round towards zero, as the compiled shift and adjust do.
    const int32_t set = sprite / 16;
    const int32_t actionNumber = sprite - set * 16;
    const ESP_CSprite* particles = gESPSprites[gESPGroups[kESPGroups - 1].mFirst + set];
    const ESP_CSprite::Action& action = particles->mActions[actionNumber];
    const int32_t view = action.mViews == 0 ? 0 : (variant & 7);
    const int32_t index = static_cast<int32_t>(action.mFirstFrame) + (frame + view * static_cast<int32_t>(action.mFrames));
    const CSpriteFileIO* records = particles->mFile;
    if (index < records->mCount && index >= 0) return reinterpret_cast<PTCLTexture*>(records->mRecords + index);
    return reinterpret_cast<PTCLTexture*>(records->mRecords);
}

// ---- ESP_InitialiseAndLoadSprites (HLSprite.h declares it)
// The sprite lists: sixteen, one for each sixteenth of the map (four by four, the last row and column taking what the
// division leaves), each linked into the map's cells; and one more, linked as map list 0x20. The original answers 1;
// nothing reads the answer.
void ESP_InitialiseAndLoadSprites(bool firstPerson, bool particlesOnly) {
    const sHField* field = gHeightfieldHeader->mHField;
    gESPDrawEnabled = 1;
    gESPF20 = nullptr;
    gESPF88 = 0.0f;
    gei_Display.mFlags |= 0x1000000;
    gSpriteHotX = 0.0f;
    gSpriteHotY = 0.0f;
    gESPFirstPerson = firstPerson ? 1 : 0;
    gESPThirdPerson = firstPerson ? 0 : 1;
    ESP_LoadSprites(particlesOnly);
    gESPF34 = 0;
    gESPMapWidth = static_cast<int32_t>(field->mF18[0]);
    gESPMapHeight = static_cast<int32_t>(field->mF18[1]);
    gESPCellsWide = gESPMapWidth / 4 + 1;
    gESPCellsHigh = gESPMapHeight / 4 + 1;
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 4; ++column) {
            CSpriteList& list = gHLSpriteLists[row * 4 + column];
            gESPListInstances[row][column] = list.Init(0x800);
            list.mCount = 0;
            const int32_t x0 = row * gESPCellsWide;
            const int32_t y0 = column * gESPCellsHigh;
            int32_t x1 = x0 + gESPCellsWide - 1;
            int32_t y1 = y0 + gESPCellsHigh - 1;
            if (row == 3) x1 = static_cast<int32_t>(field->mF18[0]) - 1;
            if (column == 3) y1 = static_cast<int32_t>(field->mF18[1]) - 1;
            gei_MapWho->Link(x0, y0, x1, y1, static_cast<CMapLinkBase*>(&list), 0);
        }
    }
    gESPGlobalInstances = gESPGlobalList.Init(0x20);
    gESPGlobalList.mCount = 0;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(&gESPGlobalList), 0x20);
}
