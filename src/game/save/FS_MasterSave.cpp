// FS_MasterSave.cpp: name proven by __sinit_FS_MasterSave_cpp in the main application.

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "helpers/GoldenTicketsHelpers.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbTime.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "MainBalance.h"
#include "MsgAdvisor.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "RideDB.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TPString.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3). PARTIAL: only
// FS_ScanThemes (0x10169310) so far; CFSGlobalMasterSave is still to do.

// Every folder inside <localPath>:data:levels is a theme: its name goes to the game. The
// result is 0 once the folder could be searched at all, -1 otherwise.
TbError FS_ScanThemes(const TbDynamicStringTemplate<char>& localPath) {
    bfl::FileSystem::CDiscFileStorage storage;
    bfl::FileSystem::CFileInfo info;
    TbDynamicStringTemplate<char> path;
    path = localPath;
    FS_AddDirectorySeparator(path);
    path += gTPGlobals.mF4088;
    int32_t result = -1;
    if (storage.Open(path.mData).Succeeded()) {
        if (storage.FindFirst(gTPGlobals.mAllFiles.mData, info).Succeeded()) {
            result = 0;
            do {
                if (FS_IsSubDirectory(info)) gGame.AddThemeName(TbDynamicStringTemplate<char>(info.mName));
            } while (storage.FindNext(info).Succeeded());
        }
    }
    return TbError(result);
}

// CFSGlobalMasterSave constructor 0x101694dc (156 bytes) and ResetGlobalData 0x10169a88 (96); existing bodies, main, unit w43.

CFSGlobalMasterSave::CFSGlobalMasterSave() { ResetGlobalData(); }

// CGoldenTicketControl (main app; unit w180): the ticket counts, the spending of a ticket on a ride, the checks that earn the local, global and
// secret tickets, the three EarnedGoldenTicket and TellIfFirst. The master save (gMasterSave) is made on first use, and the original repeats that
// test before EVERY call into it, so the helper below is called at each place the listing tests it. The three TellTheWorld overloads are in src/GoldenTicket_tell.cpp (unit w183).

// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3), third part (unit w89): the golden tickets the player has won
// and spent, the current theme, the research and first-time-player flags, the swear filter, and the destructors. The ticket
// kinds are indexes (no range check anywhere): a local ticket indexes a park's six bytes, a global one the save's four bytes and
// the park's four awards, a secret one the save's two bytes.

// ---- CFSGlobalMasterSave

// The records are deleted by the theme map's clearing; the containers are taken down with the object.
CFSGlobalMasterSave::~CFSGlobalMasterSave() { ClearThemeMap(); }

void CFSGlobalMasterSave::ResetGlobalData() {
    for (int32_t i = 0; i < 4; ++i) mGlobalTickets[i] = 0;
    for (int32_t i = 0; i < 2; ++i) mSecretTickets[i] = 0;
    mTicketsSpent = 0;
    mExtraKeys = 0;
    mEasyMode = 0;
    mSwearFilter = 1;
    mFirstTimePlayer = 1;
}

// The tickets that count: the global ones won, and the local ones of every park whose balance file reads (each park is asked
// in the map's order, which makes its file be read when it was not yet).
int32_t CFSGlobalMasterSave::HowManyTickets() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    return tickets;
}

int32_t CFSGlobalMasterSave::HowManyTicketsIncludingSecrets() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
    return tickets;
}

// The tickets that count, secret ones included, less what was spent.
int32_t CFSGlobalMasterSave::SpendingPower() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
    return tickets - mTicketsSpent;
}

// A thing is bought once: the cost is added to what was spent only the first time.
bool CFSGlobalMasterSave::SpendTicketOn(uint16_t thing, int32_t cost) {
    const bool bought = mBought.insert(thing).second;
    if (bought) mTicketsSpent += cost;
    return bought;
}

bool CFSGlobalMasterSave::HasBeenBought(uint16_t thing) const { return mBought.find(thing) != mBought.end(); }

bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeLocal type) const { return mCurrent->HaveTicket(type); }

bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeSecret type) const { return mSecretTickets[type] != 0; }

int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeLocal type) {
    if (!mCurrent->EarnedTicket(type)) return 0;
    return CheckTicketTypeWon();
}

// A global ticket won already can still move its award: when the current park does not hold the award, the first park (in the
// map's order) that holds it with a lower score loses it to the current park, and the answer is 4. A ticket won for the first
// time sets the award in the current park and answers like the others.
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeGlobal type, int32_t score) {
    if (mGlobalTickets[type] == 1) {
        if (!mCurrent->AwardInThisPark(type)) {
            for (const auto& theme : mThemes) {
                if (theme.second->AwardInThisPark(type) && theme.second->ScoreForAward(type) < score) {
                    theme.second->ClearAward(type);
                    mCurrent->SetAward(type, score);
                    return 4;
                }
            }
        }
        return 0;
    }
    mGlobalTickets[type] = 1;
    mCurrent->SetAward(type, score);
    return CheckTicketTypeWon();
}

int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeSecret type) {
    if (mSecretTickets[type] == 1) return 0;
    mSecretTickets[type] = 1;
    return CheckTicketTypeWon();
}

// FS_UserInfo.cpp and FS_MasterSave.cpp (main app), unit w43.
// GameLogin 0x1017f330 (680 bytes), SaveGlobalMasterSave 0x1017fa50 (40), GiveExtraKey 0x1016a4b4 (16), IsEasyModeUser 0x1016a4fc (8).

void CFSGlobalMasterSave::GiveExtraKey() {
    mExtraKeys = static_cast<int32_t>(static_cast<uint32_t>(mExtraKeys) + 1u);
}

bool CFSGlobalMasterSave::IsEasyModeUser() const { return mEasyMode != 0; }

// FS_MasterSave.cpp (main app), unit w48: CFSGlobalMasterSave::SetEasyModeUser 0x1016a540 (8 bytes), the byte IsEasyModeUser
// (src/FS_login.cpp) reads. In its own file so the user-slot unit's test library can link it without the login code.

// The Mac stores the LOW BYTE of the register (stb): 2 stays 2, 256 becomes 0. The parameter was a bool until the triage of
// 2026-10-12 (test_FSUserSlots easy_edge); every translated caller passes a comparison's 0 or 1.
void CFSGlobalMasterSave::SetEasyModeUser(int32_t easyMode) { mEasyMode = static_cast<uint8_t>(easyMode); }

// CFSGlobalMasterSave::Save (0x1016a584, 272 bytes), Load (0x1016a6c8, 780) and Serialise (0x1016aa08, 1,828), main app, unit w89b: the
// game-wide save file. The other members of the class are in src/FS_MasterSave_global.cpp (unit w32),
// src/FS_MasterSave_tickets.cpp and src/FS_MasterSave_signs.cpp (unit w89); the park records' own Serialise is in
// src/FS_MasterSave_park.cpp.

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

// Declared before its first use: the definition is further down.
template<> int32_t LOADVAR<char>(void* file, char* value, const char* name, int32_t noLog);

// Declared before its first use: the definition is further down.
template<> int32_t SAVEVAR<char>(void* file, char* value, const char* name, int32_t noLog);

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

// The theme becomes the current one; a theme the map does not have gets a record (and a trace line says so). The balance file
// of the current theme is read at once (the answer is not used).
void CFSGlobalMasterSave::ChangeCurrentTheme(const TbDynamicStringTemplate<char>& name) {
    const auto found = mThemes.find(name);
    if (found == mThemes.end()) {
        CTRACE(3, "Trying to change to a theme (%s) which doesn't exist, better create one then \n", name.mData);
        mCurrent = CreateNewTheme(name);
    } else {
        mCurrent = found->second;
    }
    mCurrent->CheckBalanceFile();
}

void CFSGlobalMasterSave::SetSignNameA(const char16_t* text) {
    if (mCurrent != nullptr) mCurrent->SetSignNameA(text);
}

void CFSGlobalMasterSave::SetSignNameB(const char16_t* text) {
    if (mCurrent != nullptr) mCurrent->SetSignNameB(text);
}

void CFSGlobalMasterSave::SetSwearFilter(bool on) { mSwearFilter = on; }

bool CFSGlobalMasterSave::IsSwearFilterOn() const { return mSwearFilter != 0; }

// Every record is deleted, then the map is emptied. The current theme's pointer is left as it is (it dangles).
void CFSGlobalMasterSave::ClearThemeMap() {
    for (const auto& theme : mThemes) delete theme.second;
    mThemes.clear();
}

// What a newly won ticket did: 1 nothing (no keys held, or the tickets are not a whole number of keys), 2 a key was won and no
// park costs exactly the keys held, 3 a key was won and a park costs exactly that many (it can be entered now). The total is
// tested the way the original does, `total == (total / 3) * 3`; HowManyKeys is asked again for every park.
int32_t CFSGlobalMasterSave::CheckTicketTypeWon() {
    if (HowManyKeys() > 0) {
        int32_t tickets = 0;
        for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
        for (const auto& theme : mThemes) {
            if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
        }
        for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
        if (tickets == (tickets / 3) * 3) {
            for (const auto& theme : mThemes) {
                const int32_t cost = theme.second->KeyCostToEnter();
                if (HowManyKeys() == cost) return 3;
            }
            return 2;
        }
    }
    return 1;
}

bool CFSGlobalMasterSave::HasAllResearchBeenCompleted() {
    for (const auto& theme : mThemes) {
        if (theme.second->mAllResearchCompleted == 0) return false;
    }
    return true;
}

void CFSGlobalMasterSave::SetAllResearchCompletedInCurrentTheme() {
    if (mCurrent != nullptr) mCurrent->mAllResearchCompleted = 1;
}

bool CFSGlobalMasterSave::IsFirstTimePlayer() { return mFirstTimePlayer != 0; }

void CFSGlobalMasterSave::ClearFirstTimePlayer() { mFirstTimePlayer = 0; }

// CFSGlobalParkData's loader constructor (0x1016bb04, 104 bytes) and Serialise (0x1016bd9c, 780 bytes), main app, unit w89b: one
// theme's record inside the master save file. The other members of the record are in src/FS_MasterSave_global.cpp and
// src/FS_MasterSave_tickets.cpp.

// 0x1016bb04: sets only the balance table (none) and the name, which the Mac assigns through the string's virtual assignment. Every
// other member is left as it is (unlike the other constructor, 0x1016bbc4, which clears the tickets and the flags): the loader
// reads them all with Serialise right after. The CSerialise argument is not read.
CFSGlobalParkData::CFSGlobalParkData(CSerialise& serialise, const TbDynamicStringTemplate<char>& name) : mBalance(nullptr) {
    (void)serialise;
    mName = name;
}

// ---- CFSGlobalParkData

// Takes the balance table down when the park has read one: the table's own destructor releases its rows (CBalanceTable::UnInit)
// and the memory is freed. The name is destroyed with the record.
CFSGlobalParkData::~CFSGlobalParkData() { delete mBalance; }

// 0x1016bd9c. The file holds, in this order: the six local tickets, the four awards each followed by its score, the 0x21 units of both
// sign names (interleaved: a unit of the first name, then the same index of the second), the "name changed" flag and the "all research
// completed" flag. The booleans are bytes here (the members are uint8_t); the serialiser instances take them as bool.
int32_t CFSGlobalParkData::Serialise(void* file, int32_t save, uint32_t version) {
    (void)version;
    if (save == 0) {
        for (int32_t i = 0; i < 6; ++i) {
            if (LOADVAR(file, reinterpret_cast<bool*>(&mTickets[i]), "mEarnedLocalTicket[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 4; ++i) {
            if (LOADVAR(file, reinterpret_cast<bool*>(&mAwards[i]), "mAward[i]", 1) != 0) return 1;
            if (LOADVAR(file, &mAwardScores[i], "mAwardScore[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 0x21; ++i) {
            if (LOADVAR(file, &mSignNameA[i], "mSignNameA[i]", 1) != 0) return 1;
            if (LOADVAR(file, &mSignNameB[i], "mSignNameB[i]", 1) != 0) return 1;
        }
        if (LOADVAR(file, reinterpret_cast<bool*>(&mHasSignNames), "mNameChanged", 1) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<bool*>(&mAllResearchCompleted), "mAllResearchCompleted", 1) != 0) return 1;
    } else {
        for (int32_t i = 0; i < 6; ++i) {
            if (SAVEVAR(file, reinterpret_cast<bool*>(&mTickets[i]), "mEarnedLocalTicket[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 4; ++i) {
            if (SAVEVAR(file, reinterpret_cast<bool*>(&mAwards[i]), "mAward[i]", 1) != 0) return 1;
            if (SAVEVAR(file, &mAwardScores[i], "mAwardScore[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 0x21; ++i) {
            if (SAVEVAR(file, &mSignNameA[i], "mSignNameA[i]", 1) != 0) return 1;
            if (SAVEVAR(file, &mSignNameB[i], "mSignNameB[i]", 1) != 0) return 1;
        }
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mHasSignNames), "mNameChanged", 1) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mAllResearchCompleted), "mAllResearchCompleted", 1) != 0) return 1;
    }
    return 0;
}

bool CFSGlobalParkData::HaveTicket(GoldenTicketTypeLocal type) const { return mTickets[type] != 0; }

// A ticket counts as won only when its byte is exactly 1; any other value (the saved file could hold one) is won again.
bool CFSGlobalParkData::EarnedTicket(GoldenTicketTypeLocal type) {
    if (mTickets[type] == 1) return false;
    mTickets[type] = 1;
    return true;
}

bool CFSGlobalParkData::AwardInThisPark(GoldenTicketTypeGlobal type) const { return mAwards[type] != 0; }

int32_t CFSGlobalParkData::ScoreForAward(GoldenTicketTypeGlobal type) const {
    if (mAwards[type] != 0) return mAwardScores[type];
    return -1;
}

void CFSGlobalParkData::ClearAward(GoldenTicketTypeGlobal type) { mAwards[type] = 0; }

void CFSGlobalParkData::SetAward(GoldenTicketTypeGlobal type, int32_t score) {
    mAwards[type] = 1;
    mAwardScores[type] = score;
}

// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3), fourth part (unit w89): the park record's sign texts, the two
// lines of its gate sign. The player may name the sign (two lines of up to 0x21 characters); otherwise the lines are the ride
// database's sign text for the park's gate object, whose number the park's balance table holds.

// The work area of GetSignNameA (0x10380a08, 0x21 characters; the pointer to it is the global at 0x102637f8). GetFullName's own
// is the next one at 0x10380a4a.
char16_t gParkSignNameScratch[0x21];

// Without sign names the first line is the ride database's for the gate object; a balance file that cannot be read gives an
// empty text (written with the caller's size, unlike GetFullName's). With sign names it is the player's first line (an empty text
// when it is empty); a name of two or more characters that ends in two hyphens loses its last character (cut in the work area
// from a copy of at most 31 characters).
void CFSGlobalParkData::GetSignNameA(char16_t* buffer, uint32_t size) {
    if (mHasSignNames == 0) {
        if (CheckBalanceFile()) gRideDatabase.GetSignNameA(static_cast<uint16_t>(mBalance->mValues[4].mInteger), buffer, size);
        else swprintf(buffer, size, u"");
        return;
    }
    const uint32_t length = wcslen(mSignNameA);
    if (length == 0) {
        swprintf(buffer, size, u"");
        return;
    }
    if (static_cast<int32_t>(length) > 1 && mSignNameA[length - 1] == u'-' && mSignNameA[length - 2] == u'-') {
        swprintf(gParkSignNameScratch, 0x20, u"%ls", mSignNameA);
        gParkSignNameScratch[length - 1] = 0;
        swprintf(buffer, size, u"%ls", gParkSignNameScratch);
        return;
    }
    swprintf(buffer, size, u"%ls", mSignNameA);
}

// The second line: the ride database's, or the player's as it is.
void CFSGlobalParkData::GetSignNameB(char16_t* buffer, uint32_t size) {
    if (mHasSignNames == 0) {
        if (CheckBalanceFile()) gRideDatabase.GetSignNameB(static_cast<uint16_t>(mBalance->mValues[4].mInteger), buffer, size);
        else swprintf(buffer, size, u"");
        return;
    }
    swprintf(buffer, size, u"%ls", mSignNameB);
}
// The player's text, at most 0x21 characters (not terminated when the text has that many or more, in which case the name runs on
// into the second line's array), and the park has sign names from now on.
void CFSGlobalParkData::SetSignNameA(const char16_t* text) {
    wcsncpy(mSignNameA, text, 0x21);
    mHasSignNames = 1;
}

void CFSGlobalParkData::SetSignNameB(const char16_t* text) {
    wcsncpy(mSignNameB, text, 0x21);
    mHasSignNames = 1;
}

// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3), second part (unit w32): the game-wide save's constructor, its
// reset, the keys the player holds and the theme records, and the park record's constructor, its balance file and its name.
// The ticket bookkeeping and the sign names are in FS_MasterSave_tickets.cpp and FS_MasterSave_signs.cpp (unit w89); the loader and
// the saver of the same range are not translated yet.

// The rows of the park balance table's description (0x10294614): CanEarnTicketsInTheme, CanSpendTicketsInTheme and
// CostToEnter are integers, GateObjectId an integer from 0; "Tickets", "Keys" and "ParkName" are the group headings that follow
// their members. The row of type 12 ends the table.
const BalanceDescription gGlobalParkDataBalanceDescriptions[11] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CanEarnTicketsInTheme", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CanSpendTicketsInTheme", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "Tickets", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CostToEnter", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "Keys", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "GateObjectId", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "ParkName", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CGlobalParkDataBalance::GetBalanceDescription(int32_t row) {
    return &gGlobalParkDataBalanceDescriptions[row];
}
BalanceValue* CGlobalParkDataBalance::GetBalanceValuesStart() { return mValues; }

namespace {
// The text part of the sign names' work area (0x10380a4a): GetFullName copies the first sign name here to cut its hyphen off.
// The Mac's array is 0x21 characters, the size of a sign name.
char16_t gParkNameScratch[0x21];   // 0x10380a4a
}  // namespace

// ---- CFSGlobalMasterSave

int32_t CFSGlobalMasterSave::HowManyKeys() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
    return mExtraKeys + tickets / 3;
}

CFSGlobalParkData* CFSGlobalMasterSave::GetInfoOn(const TbDynamicStringTemplate<char>& name) {
    const auto found = mThemes.find(name);
    if (found == mThemes.end()) {
        CTRACE(3, "Trying to get info from a theme (%s) which doesn't exist, better create one then \n", name.mData);
        CFSGlobalParkData* park = CreateNewTheme(name);
        return park->CheckBalanceFile() ? park : nullptr;
    }
    return found->second->CheckBalanceFile() ? found->second : nullptr;
}

CFSGlobalParkData* CFSGlobalMasterSave::CreateNewTheme(const TbDynamicStringTemplate<char>& name) {
    if (mThemes.find(name) != mThemes.end()) {
        CTRACE(3, "Trying to create Global save info for a theme (%s) which is already in the map (FS_LoadSave.cpp)\n", name.mData);
    }
    CFSGlobalParkData* park = new CFSGlobalParkData(name);
    std::pair<const TbDynamicStringTemplate<char>, CFSGlobalParkData*> entry(name, park);
    mThemes.insert(entry);
    return park;
}
// ---- CFSGlobalParkData

// The record of a theme with nothing won and no balance table; the name is copied. (The Mac's constructor sets the tickets,
// the awards, the sign flag and the first character of the sign names; the awards' scores are left as they are.)
CFSGlobalParkData::CFSGlobalParkData(const TbDynamicStringTemplate<char>& name) : mBalance(nullptr) {
    for (int32_t i = 0; i < 6; ++i) mTickets[i] = 0;
    for (int32_t i = 0; i < 4; ++i) mAwards[i] = 0;
    mName = name;
    mHasSignNames = 0;
    mAllResearchCompleted = 0;
    mSignNameA[0] = 0;
    mSignNameB[0] = 0;
}

int32_t CFSGlobalParkData::HowManyTickets() const {
    int32_t count = 0;
    for (int32_t i = 0; i < 6; ++i) count += mTickets[i] != 0 ? 1 : 0;
    return count;
}

int32_t CFSGlobalParkData::KeyCostToEnter() {
    if (!CheckBalanceFile()) return -1;
    return mBalance->mValues[3].mInteger;
}

// The balance table is made and read the first time anyone asks. When the file cannot be read the table is taken down again
// (so the next question tries the file again) and a trace line says so.
bool CFSGlobalParkData::CheckBalanceFile() {
    if (mBalance != nullptr) return true;
    mBalance = new CGlobalParkDataBalance;
    char path[260];
    std::sprintf(path, "data:levels:%s:global.sam", mName.mData);
    if (mBalance->Load(path, CBalanceTable::kLoadFresh) != 0) return true;
    delete mBalance;
    mBalance = nullptr;
    CTRACE(3, "!!!!!\nLoading the global.sam went horribly wrong! Shout at Bjarne!\n!!!!!");
    return false;
}

// The sign names, when the player gave the park some: both lines joined with a blank, or without when the first ends with a
// hyphen (the hyphen is dropped, and the line is cut to 31 characters first); one line alone as it is. Otherwise the park's name
// is that of its gate object in the ride database. When the balance file cannot be read nothing is written (the Mac formats an
// empty text into a buffer of size 0).
void CFSGlobalParkData::GetFullName(char16_t* buffer, uint32_t size) {
    if (mHasSignNames != 0) {
        const uint32_t length = wcslen(mSignNameA);
        if (length == 0) {
            wcsncpy(buffer, mSignNameB, size);
        } else if (mSignNameA[length - 1] == u'-') {
            gParkNameScratch[0x20] = 0;
            swprintf(gParkNameScratch, 0x20, u"%ls", mSignNameA);
            gParkNameScratch[length - 1] = 0;
            swprintf(buffer, size, u"%ls%ls", gParkNameScratch, mSignNameB);
        } else if (wcslen(mSignNameB) == 0) {
            wcsncpy(buffer, mSignNameA, size);
        } else {
            swprintf(buffer, size, u"%ls %ls", mSignNameA, mSignNameB);
        }
        return;
    }
    if (CheckBalanceFile()) gRideDatabase.GetTypeName(static_cast<uint16_t>(mBalance->mValues[4].mInteger), buffer, size);
    else swprintf(buffer, 0, u"");
}

// Unit v51117: FS_MasterSave.cpp's static initialiser, the online callbacks that make the online system on first use, and keyboard actions.

// 0x1016f35c
void __sinit_FS_MasterSave_cpp() {
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyWString);
    auto* s = new (gMasterSaveString) TbDynamicStringTemplate<char16_t>(
        reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gMasterSaveWideText) + 0x5f));
    __register_global_object(s, destroy, gMasterSaveNode);
}

// Unit v51065: the online lists' small members, the park lists' GetList for the top ten and the by-park-id lists, the login callbacks,
// two destructors (the city list's and the theme list's base), the park instance of LoggedInGetList and the FTP download getters.

// 0x1016e988 (4): blr.
void AllocatorUShortDestroy(void*, uint16_t*) {}

// Unit v51123: small CFSOnlineSystem members (the FTP post-login callbacks, the login getters, Login, Logout), the park, server and theme
// instances of CFSOnlineSystemList::CompleteGetList, two std::__tree helpers and two status panel members.

__attribute__((weak, noinline)) CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list) { return list->RemoveHead(); }
#define TPW_COMPLETE_GET_LIST(LISTTYPE, RECTYPE, RECEIVEDTYPE, REMOVEHEAD, DELETER)                              \
    template<> void LISTTYPE::CompleteGetList(int32_t error) {                                                    \
        bool hadError = mF18 != 0;                                                                                \
        if (hadError) {                                                                                           \
            AddUIError(mF18, nullptr);                                                                            \
            mF18 = 0;                                                                                             \
        }                                                                                                         \
        int32_t callback = mF1C;                                                                                  \
        int32_t context = mF20;                                                                                   \
        if (error == 0 && !hadError && mF14 != 0) {                                                               \
            while (mList->GetCount() != 0) {                                                                      \
                RECTYPE* record = REMOVEHEAD(mList);                                                              \
                if (record != nullptr) DELETER(record);                                                           \
            }                                                                                                     \
            mF24 = 0;                                                                                             \
            mF28 = 0;                                                                                             \
            mF28 = 1;                                                                                             \
            if (mReceived != nullptr) {                                                                           \
                StoreReceivedList(mReceived);                                                                     \
                mF24 = mReceived->mF08;                                                                           \
                RECEIVEDTYPE* received = mReceived;                                                               \
                if (received != nullptr) {                                                                        \
                    --received->mRefCount;                                                                        \
                    if (received->mRefCount == 0) {                                                               \
                        if (received->mInfos != nullptr) delete[] received->mInfos;                               \
                        received->mInfos = nullptr;                                                               \
                        ::operator delete(static_cast<void*>(received));                                          \
                    }                                                                                             \
                    mReceived = nullptr;                                                                          \
                }                                                                                                 \
            }                                                                                                     \
        }                                                                                                         \
        int32_t result = 0;                                                                                       \
        if (error != 0 || hadError || mF28 == 0) {                                                                \
            if (mF08 != 0) {                                                                                      \
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();                              \
                gOnlineSystem->Logout();                                                                          \
            }                                                                                                     \
            result = 1;                                                                                           \
        }                                                                                                         \
        if (callback != 0) gOnlineListCallbackCaller(callback, result, context, mList);                           \
    }

#define TPW_CALL_DELETER_PARK(record) gParkRecordDeleter(record)
#define TPW_CALL_DELETER_SERVER(record) gServerRecordDeleter(record)
#define TPW_CALL_DELETER_THEME(record) gThemeRecordDeleter(record)

// ---- std::__tree<unsigned short> helpers
// 0x1016eaf0: the header words: a copy of the global word, a cleared word, and a pointer to the cleared word (the comparator byte the Mac
// reads and stores at 8 is overwritten at once).
namespace macstd {
void* gTreeUShortWord = nullptr;   // 0x102a722c (original name unknown)
TreeUShort::TreeUShort(const LessUShort& less, const AllocUShort& alloc) {
    (void)less;
    (void)alloc;
    mF00 = gTreeUShortWord;
    mF04 = nullptr;
    mF08 = &mF04;
}
// 0x1016e8b0: gives the node back (the count is unused).
void AllocUShortNode::deallocate(void* node, unsigned long count) {
    (void)count;
    ::operator delete(node);
}
}  // namespace macstd

// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.

template<> int32_t LOADVAR<char>(void* file, char* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1016d2c8, 228 bytes
}
template<> int32_t SAVEVAR<char>(void* file, char* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1016d3d4, 236 bytes
}
