// CFSGlobalMasterSave::Save (0x1016a584, 272 bytes), Load (0x1016a6c8, 780) and Serialise (0x1016aa08, 1,828), main app, unit w89b: the
// game-wide save file. The other members of the class are in src/FS_MasterSave_global.cpp (unit w32),
// src/FS_MasterSave_tickets.cpp and src/FS_MasterSave_signs.cpp (unit w89); the park records' own Serialise is in
// src/FS_MasterSave_park.cpp.
#include <cstdio>
#include <cstring>

#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "LbFile.h"
#include "Serialise.h"
#include "TPSettings.h"
#include "Text.h"

namespace {
constexpr uint32_t kLbFileWriteMaster = 0x40000010;   // the mode Save opens with
constexpr uint32_t kLbFileReadMaster = 0x80000000;    // the mode Load opens with (LoadGlobal's has bit 0 set as well)
constexpr int32_t kMasterSaveVersion = 0xc;           // what Save writes; Load refuses a lower one
}  // namespace

// 0x1016a584. 1 when the master save was written, 0 when the file could not be opened or a value could not be written (the file is
// closed in every case but the first).
int32_t CFSGlobalMasterSave::Save(const char* path) {
    void* file;
    if (!LbFile_Open(file, path, kLbFileWriteMaster).Succeeded()) {
        CTRACE(3, "Failed to open %s", path);
        return 0;
    }
    int32_t version = kMasterSaveVersion;
    if (SAVEVAR(file, &version, "version", 1) != 0) {
        LbFile_Close(file);
        return 0;
    }
    if (Serialise(file, 1, static_cast<uint32_t>(version)) != 0) {
        LbFile_Close(file);
        return 0;
    }
    LbFile_Close(file);
    return 1;
}

// 0x1016a6c8. Resets the save and empties the theme map before the file is opened, so a file that cannot be read leaves an empty save.
// After a successful read the global gMasterSave is made when there is none (the Mac's inlined constructor), and when ITS swear filter
// flag is on the word lists are read into THIS object (the filter's lists are the first two of the master save); if that fails the
// global's flag is cleared. `this` and gMasterSave are the same object in the game.
int32_t CFSGlobalMasterSave::Load(const char* path) {
    ResetGlobalData();
    ClearThemeMap();
    void* file;
    if (!LbFile_Open(file, path, kLbFileReadMaster).Succeeded()) {
        CTRACE(3, "Couldn't open file %s", path);
        return 0;
    }
    int32_t version;
    if (LOADVAR(file, &version, "version", 1) != 0) {
        LbFile_Close(file);
        return 0;
    }
    if (static_cast<uint32_t>(version) < static_cast<uint32_t>(kMasterSaveVersion)) {
        LbFile_Close(file);
        return 0;
    }
    if (Serialise(file, 0, static_cast<uint32_t>(version)) != 0) {
        LbFile_Close(file);
        return 0;
    }
    LbFile_Close(file);
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    if (gMasterSave->IsSwearFilterOn()) {
        // (the argument order is the Mac's: the data path, then the language, for each file)
        char wordFile[1024];
        char allowedFile[1024];
        const char* dataPath = gTPFileInfo.GetDataPath().mData;
        const char* language = gText.GetLanguageName();
        std::snprintf(wordFile, sizeof wordFile, "%s:Language:%s:swears.txt", dataPath, language);
        dataPath = gTPFileInfo.GetDataPath().mData;
        language = gText.GetLanguageName();
        std::snprintf(allowedFile, sizeof allowedFile, "%s:Language:%s:alloweds.txt", dataPath, language);
        if (!reinterpret_cast<SwearFilter*>(this)->Init(wordFile, allowedFile, true)) {
            if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
            gMasterSave->SetSwearFilter(false);
        }
    }
    return 1;
}

// 0x1016aa08. The body of the file, written (save != 0) or read in the same order: the four global and two secret tickets, the keys
// spent and the extra keys, the easy mode, swear filter and first-time-player flags, the number of themes and for each its name (a
// length and the characters, one by one) and its record, the player settings, and the number of bought things with each one's id. A
// failed read or write answers 1 at once, whatever has been done is kept (and a name buffer or record being read is leaked). The
// version is passed on to the records and the settings and decides nothing here. Quirks: the booleans are bytes (the members are
// uint8_t, handed to the bool instances), a theme that is already in the map when loaded (a repeated name) is dropped after a trace
// line and the load fails, and a name keeps the length of its text up to the first NUL.
int32_t CFSGlobalMasterSave::Serialise(void* file, int32_t save, uint32_t version) {
    if (save == 0) {
        for (int32_t i = 0; i < 4; ++i) {
            if (LOADVAR(file, reinterpret_cast<bool*>(&mGlobalTickets[i]), "mEarnedGlobalTicket[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 2; ++i) {
            if (LOADVAR(file, reinterpret_cast<bool*>(&mSecretTickets[i]), "mEarnedSecretTicket[i]", 1) != 0) return 1;
        }
        if (LOADVAR(file, &mTicketsSpent, "mSpentTickets", 1) != 0) return 1;
        if (LOADVAR(file, &mExtraKeys, "mExtraKeys", 1) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<bool*>(&mEasyMode), "mEasyModeUser", 1) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<bool*>(&mSwearFilter), "mSwearFilterOn", 1) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<bool*>(&mFirstTimePlayer), "mFirstTimePlayer", 1) != 0) return 1;
        int32_t themes;
        if (LOADVAR(file, &themes, "size", 1) != 0) return 1;
        for (int32_t i = 0; i < themes; ++i) {
            uint32_t length;
            if (LOADVAR(file, &length, "name_str_len", 1) != 0) return 1;
            char* characters = new char[length + 1];
            for (uint32_t j = 0; j < length; ++j) {
                if (LOADVAR(file, characters + j, "str[j]", 1) != 0) return 1;
            }
            characters[length] = '\0';
            const TbDynamicStringTemplate<char> name(characters);
            delete[] characters;
            CSerialise serialise;
            CFSGlobalParkData* park = new CFSGlobalParkData(serialise, name);
            if (park->Serialise(file, 0, version) != 0) return 1;
            if (!mThemes.insert(std::make_pair(name, park)).second) {
                CTRACE(3, "Something went wrong when trying to insert the recently serialised (loaded) theme into the map\n");
                return 1;
            }
        }
        if (gPlayerSettings.SerialiseLocal(file, 0, version) != 0) return 1;
        int32_t bought;
        if (LOADVAR(file, &bought, "size", 1) != 0) return 1;
        for (int32_t i = 0; i < bought; ++i) {
            uint16_t thing;
            if (LOADVAR(file, &thing, "rideId", 1) != 0) return 1;
            mBought.insert(thing);
        }
    } else {
        for (int32_t i = 0; i < 4; ++i) {
            if (SAVEVAR(file, reinterpret_cast<bool*>(&mGlobalTickets[i]), "mEarnedGlobalTicket[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 2; ++i) {
            if (SAVEVAR(file, reinterpret_cast<bool*>(&mSecretTickets[i]), "mEarnedSecretTicket[i]", 1) != 0) return 1;
        }
        if (SAVEVAR(file, &mTicketsSpent, "mSpentTickets", 1) != 0) return 1;
        if (SAVEVAR(file, &mExtraKeys, "mExtraKeys", 1) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mEasyMode), "mEasyModeUser", 1) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mSwearFilter), "mSwearFilterOn", 1) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mFirstTimePlayer), "mFirstTimePlayer", 1) != 0) return 1;
        int32_t themes = static_cast<int32_t>(mThemes.size());
        if (SAVEVAR(file, &themes, "size", 1) != 0) return 1;
        for (const auto& theme : mThemes) {
            uint32_t length = static_cast<uint32_t>(theme.first.Length());
            if (SAVEVAR(file, &length, "str_length", 1) != 0) return 1;
            char* characters = new char[length + 1];
            std::strcpy(characters, theme.first.mData);
            for (uint32_t j = 0; j < length; ++j) {
                if (SAVEVAR(file, characters + j, "str[i]", 1) != 0) return 1;
            }
            delete[] characters;
            if (theme.second->Serialise(file, save, version) != 0) return 1;
        }
        if (gPlayerSettings.SerialiseLocal(file, save, version) != 0) return 1;
        int32_t bought = static_cast<int32_t>(mBought.size());
        if (SAVEVAR(file, &bought, "size", 1) != 0) return 1;
        for (uint16_t id : mBought) {
            uint16_t thing = id;
            if (SAVEVAR(file, &thing, "rideId", 1) != 0) return 1;
        }
    }
    return 0;
}
