// FS_UserInfo.cpp: name proven by __sinit_FS_UserInfo_cpp in the main application.

#include "bfl_FileSystem.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "TPGlobals.h"
#include "TPSettings.h"
#include "TPString.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

// CFSUserInfo constructor, main 0x1017de54 (108 bytes); existing body separated for unit w43.

CFSUserInfo::CFSUserInfo() { mGameUser = -1; }

// FS_UserInfo.cpp (decompiled from the main app of SimTheme Park 2.04b3: the start-up part
// of 0x1017de54..0x1017f9fc and __sinit_FS_UserInfo_cpp at 0x1017fd58). PARTIAL: see
// FS_UserInfo.h for the functions still to do.
//
// The std::list<std::string>::clear instance at 0x1017fc04 (the address book's inline
// destructor) is the host's std::list.

CFSUserInfo* gUserInfo;   // 0x10380b34

// __sinit_FS_UserInfo_cpp, in construction order.
TbDynamicStringTemplate<char16_t> gFSUserInfoUpload(u"upload");    // 0x10380b58
TbDynamicStringTemplate<char16_t> gFSUserInfoGmsFile(u"gms.dat");  // 0x10380b38

namespace {

constexpr int32_t kLoginSlots = 4;

// A folder name of the users folder is "<slot><user name>" with the slot from 1.
bool IsSlotFolder(const bfl::FileSystem::CFileInfo& info) {
    return std::strlen(info.mName) > 1 && FS_IsSubDirectory(info) &&
           std::isdigit(static_cast<unsigned char>(info.mName[0])) != 0;
}

}  // namespace

CFSUserInfo::~CFSUserInfo() {}

void CFSUserInfo::GameLoginInit() {
    for (int32_t i = 0; i < kLoginSlots; ++i) {
        mUsers[i].mName = u"";
        mUsers[i].mEasyMode = 0;
    }
    const TbDynamicStringTemplate<char16_t> saveDirectory(ReturnPathToSaveDirectory());
    TbDynamicStringTemplate<char> save;
    UniStringToString(save, saveDirectory);
    const TbDynamicStringTemplate<char16_t> usersDirectory(ReturnPathToUsersDirectory());
    TbDynamicStringTemplate<char> users;
    UniStringToString(users, usersDirectory);
    const TbDynamicStringTemplate<char16_t> onlineDirectory(ReturnOnlinePath());
    TbDynamicStringTemplate<char> online;
    UniStringToString(online, onlineDirectory);

    bfl::FileSystem::CDiscFileStorage storage;
    if (!storage.Open(save.mData).Succeeded()) {
        LbFile_CreateDirectory(save.mData);
        LbFile_CreateDirectory(users.mData);
        LbFile_CreateDirectory(online.mData);
        return;
    }
    if (!storage.Open(users.mData).Succeeded()) {
        LbFile_CreateDirectory(users.mData);
    } else {
        bfl::FileSystem::CFileInfo info;
        if (storage.FindFirst(gTPGlobals.mAllFiles.mData, info).Succeeded()) {
            do {
                if (!IsSlotFolder(info)) continue;
                const char digit[2] = {info.mName[0], '\0'};
                const int32_t number = std::atoi(digit);
                if (number < 1 || number > kLoginSlots) continue;
                TbDynamicStringTemplate<char> name(info.mName);
                name = name.Right(static_cast<uint32_t>(name.Length() - 1));
                const auto slot = static_cast<EFSUserInfoGameUser>(number - 1);
                StringToUniString(mUsers[slot].mName, name);
                const TbDynamicStringTemplate<char16_t> userPath(ReturnGameLoginSlotUsersPath(slot));
                TbDynamicStringTemplate<char16_t> masterSaveName(userPath);
                masterSaveName += gTPGlobals.mWideSeparator;
                masterSaveName += gFSUserInfoGmsFile;
                TbDynamicStringTemplate<char> masterSavePath;
                UniStringToString(masterSavePath, masterSaveName);
                // the user's master save says whether the user plays in easy mode
                if (gMasterSave != nullptr) delete gMasterSave;
                gMasterSave = nullptr;
                CFSGlobalMasterSave* masterSave = new CFSGlobalMasterSave;
                gMasterSave = masterSave;
                masterSave->Load(masterSavePath.mData);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
                mUsers[slot].mEasyMode = gMasterSave->IsEasyModeUser() ? 1 : 0;
                if (gMasterSave != nullptr) delete gMasterSave;
                gMasterSave = nullptr;
                CheckAllThemeDirectoriesExist(userPath);
            } while (storage.FindNext(info).Succeeded());
        }
    }
    if (!storage.Open(online.mData).Succeeded()) LbFile_CreateDirectory(online.mData);
}

void CFSUserInfo::CheckAllThemeDirectoriesExist(const TbDynamicStringTemplate<char16_t>& path) {
    for (auto* node = gGame.mThemes.mHead; node != nullptr;) {
        const TbDynamicStringTemplate<char>* theme = node->mData;
        node = node->mNext;
        TbDynamicStringTemplate<char> directory;
        UniStringToString(directory, path);
        directory += gTPGlobals.mSeparator;
        directory += *theme;
        bfl::FileSystem::CDiscFileStorage storage;
        if (!storage.Open(directory.mData).Succeeded()) LbFile_CreateDirectory(directory.mData);
    }
}

int32_t CFSUserInfo::GetNumValidGameLoginSlots() {
    int32_t slots = 0;
    for (int32_t i = 0; i < kLoginSlots; ++i)
        if (mUsers[i].mName.Length() != 0) ++slots;
    return slots;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnPathToSaveDirectory() {
    TbDynamicStringTemplate<char16_t> path;
    StringToUniString(path, gTPFileInfo.GetLocalPath());
    path += gTPGlobals.mSaveFolder;
    return path;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnPathToUsersDirectory() {
    TbDynamicStringTemplate<char16_t> path(ReturnPathToSaveDirectory());
    path += gTPGlobals.mWideSeparator;
    path += gTPGlobals.mUsersFolder;
    return path;
}

const TbStringBase<char16_t>* CFSUserInfo::GetGameLoginSlotUserName(EFSUserInfoGameUser slot) {
    return mUsers[slot].mName.Length() != 0 ? &mUsers[slot].mName : nullptr;
}

int32_t CFSUserInfo::IsGameLoginSlotUserEasyMode(EFSUserInfoGameUser slot) { return mUsers[slot].mEasyMode; }

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnGameLoginSlotUsersDirectoryName(EFSUserInfoGameUser slot) {
    char16_t number[16];
    NS_MacDoze::_itow(slot + 1, number, 10);
    TbDynamicStringTemplate<char16_t> name(number);
    name += *GetGameLoginSlotUserName(slot);   // inline in the original
    return name;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnGameLoginSlotUsersPath(EFSUserInfoGameUser slot) {
    TbDynamicStringTemplate<char16_t> path(ReturnPathToUsersDirectory());
    path += gTPGlobals.mWideSeparator;
    path += ReturnGameLoginSlotUsersDirectoryName(slot);
    return path;
}

// FS_UserInfo.cpp (main app), unit w48: creating and deleting a player on disk, the user folders
// and the write-back of the global master save.
// CreateNewGameLoginUserInSlot 0x1017ee38 (524 bytes), DeleteGameLoginUserInSlot 0x1017ed10 (212),
// CreateUsersDirectories 0x1017f0b8 (544), InternalSaveGlobalMasterSave 0x1017fab0 (256).

namespace {

// The "easymode" file name (the constant at TOC 0x1026389c, data 0x1024d85e).
constexpr const char kEasyModeFile[] = "easymode";

// The original copies the returned path and destroys the return temporary at once.
TbDynamicStringTemplate<char16_t> SlotPath(CFSUserInfo* user, EFSUserInfoGameUser slot) {
    const TbDynamicStringTemplate<char16_t> returned = user->ReturnGameLoginSlotUsersPath(slot);
    TbDynamicStringTemplate<char16_t> path(returned);
    return path;
}

}  // namespace

void CFSUserInfo::DeleteGameLoginUserInSlot(EFSUserInfoGameUser slot) {
    TbDynamicStringTemplate<char16_t> path(SlotPath(this, slot));
    TbDynamicStringTemplate<char> narrowPath;
    UniStringToString(narrowPath, path);
    FS_DelTree(narrowPath);
    mUsers[slot].mName = u"";   // the name's virtual at 0x20 with the empty wide string (0x1024d868)
    mUsers[slot].mEasyMode = 0;
}

void CFSUserInfo::CreateNewGameLoginUserInSlot(EFSUserInfoGameUser slot, const TbDynamicStringTemplate<char16_t>& name,
                                               int32_t easyMode) {
    TFSUserInfoGameLoginUser& user = mUsers[slot];
    if (user.mName.Length() != 0) {
        // the slot already holds a player: its folder goes first
        TbDynamicStringTemplate<char16_t> oldPath(SlotPath(this, slot));
        TbDynamicStringTemplate<char> narrowPath;
        UniStringToString(narrowPath, oldPath);
        FS_DelTree(narrowPath);
        user.mName = u"";
        user.mEasyMode = 0;
    }
    user.mName = name;   // the name's virtual at 0x1c
    user.mEasyMode = easyMode;
    TbDynamicStringTemplate<char16_t> path(SlotPath(this, slot));
    CreateUsersDirectories(path, easyMode);
    // the master save in memory is dropped (a Load'ed one is lost) and a blank one made
    delete gMasterSave;
    gMasterSave = nullptr;
    gMasterSave = new CFSGlobalMasterSave;
    gMasterSave->SetEasyModeUser(user.mEasyMode != 0);
    InternalSaveGlobalMasterSave(slot);
}

void CFSUserInfo::CreateUsersDirectories(const TbDynamicStringTemplate<char16_t>& path, int32_t easyMode) {
    TbDynamicStringTemplate<char> narrowPath;
    UniStringToString(narrowPath, path);
    LbFile_CreateDirectory(narrowPath.mData);
    POSITION position = gGame.mThemes.GetHeadPosition();
    while (position != nullptr) {
        TbDynamicStringTemplate<char>* theme = gGame.mThemes.GetNext(position);
        TbDynamicStringTemplate<char> themePath(narrowPath);
        themePath += gTPGlobals.mSeparator;
        themePath += *theme;
        LbFile_CreateDirectory(themePath.mData);
        if (easyMode != 0) {
            // the park's ready-made easy mode file in the data folder is copied into the player's theme folder
            TbDynamicStringTemplate<char> source;
            source = gTPFileInfo.GetDataPath();
            source += gTPGlobals.mSeparator;
            source += gTPGlobals.mLevelsFolder;
            source += gTPGlobals.mSeparator;
            source += *theme;
            source += gTPGlobals.mSeparator;
            source += kEasyModeFile;
            source += gTPGlobals.mF4434;
            if (LbFile_Exists(source.mData)) {
                TbDynamicStringTemplate<char> target;
                target = themePath;
                target += gTPGlobals.mSeparator;
                target += kEasyModeFile;
                target += gTPGlobals.mF4434;
                LbFile_Copy(source.mData, target.mData);
            }
        }
    }
}

// FS_UserInfo.cpp and FS_MasterSave.cpp (main app), unit w43.
// GameLogin 0x1017f330 (680 bytes), SaveGlobalMasterSave 0x1017fa50 (40), GiveExtraKey 0x1016a4b4 (16), IsEasyModeUser 0x1016a4fc (8).

namespace {
CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

// The original copies the returned path and destroys the return temporary before loading mail.
TbDynamicStringTemplate<char16_t> LoginPath(CFSUserInfo* user, EFSUserInfoGameUser slot) {
    const TbDynamicStringTemplate<char16_t> returned = user->ReturnGameLoginSlotUsersPath(slot);
    TbDynamicStringTemplate<char16_t> path(returned);
    return path;
}
}  // namespace

void CFSUserInfo::GameLogin(EFSUserInfoGameUser slot) {
    mGameUser = slot;
    TbDynamicStringTemplate<char16_t> path(LoginPath(this, slot));
    TbDynamicStringTemplate<char> narrowPath;
    UniStringToString(narrowPath, path);
    mOutbox.Load(narrowPath.mData);
    mAddressBook.Load(narrowPath.mData);
    delete gMasterSave;
    gMasterSave = nullptr;
    path += gTPGlobals.mWideSeparator;
    path += gFSUserInfoGmsFile;
    TbDynamicStringTemplate<char> savePath;
    UniStringToString(savePath, path);
    MasterSave()->Load(savePath.mData);
    if (TheGameType().mType != kGameTypeOnline) {
        const bool easy = MasterSave()->IsEasyModeUser();
        TheGameType().SetGameType(easy ? kGameTypeEasy : kGameTypeNormal);
    }
    POSITION position = gGame.mThemes.GetHeadPosition();
    while (position != nullptr) {
        TbDynamicStringTemplate<char>* theme = gGame.mThemes.GetNext(position);
        MasterSave()->GetInfoOn(*theme);
    }
}

// CFSUserInfo::GameLogout (main app 0x1017f61c, 424 bytes): the logged-in player's address book, outbox and global master save
// are written to the player's folder, the master save in memory is dropped, the settings are saved to <save>:<:><Config.tcf>
// and the slot is cleared (mGameUser = -1).

void CFSUserInfo::GameLogout() {
    // the slot's path: built in a temporary, copied (the original copies), the temporary destroyed
    TbDynamicStringTemplate<char16_t> slotPath(
        static_cast<const TbDynamicStringTemplate<char16_t>&>(ReturnGameLoginSlotUsersPath(static_cast<EFSUserInfoGameUser>(mGameUser))));
    TbDynamicStringTemplate<char> slotPathNarrow;
    UniStringToString(slotPathNarrow, slotPath);
    mAddressBook.Save(slotPathNarrow.mData);   // the answers of both saves are not used
    mOutbox.Save();
    SaveGlobalMasterSave();
    if (gMasterSave != nullptr) delete gMasterSave;
    gMasterSave = nullptr;

    TbDynamicStringTemplate<char16_t> settingsPath;
    TbDynamicStringTemplate<char> settingsPathNarrow;
    settingsPath = ReturnPathToSaveDirectory();
    settingsPath += gTPGlobals.mWideSeparator;
    settingsPath += gTPGlobals.mConfigFile;
    UniStringToString(settingsPathNarrow, settingsPath);
    gPlayerSettings.SaveGlobal(settingsPathNarrow);
    mGameUser = -1;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPath() {
    return ReturnGameLoginSlotUsersPath(static_cast<EFSUserInfoGameUser>(mGameUser));
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPathWithTheme() {
    TbDynamicStringTemplate<char16_t> path(ReturnUsersPath());
    path += gTPGlobals.mWideSeparator;
    const TbDynamicStringTemplate<char> theme(gGame.GetCurrentThemeName());
    TbDynamicStringTemplate<char16_t> wideTheme;
    StringToUniString(wideTheme, theme);
    path += wideTheme;
    return path;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnOnlinePath() {
    TbDynamicStringTemplate<char16_t> path(ReturnPathToSaveDirectory());
    path += gTPGlobals.mWideSeparator;
    path += gTPGlobals.mOnlineFolder;
    return path;
}

void CFSUserInfo::SaveGlobalMasterSave() {
    InternalSaveGlobalMasterSave(static_cast<EFSUserInfoGameUser>(mGameUser));
}

void CFSUserInfo::InternalSaveGlobalMasterSave(EFSUserInfoGameUser slot) {
    TbDynamicStringTemplate<char16_t> path(SlotPath(this, slot));
    path += gTPGlobals.mWideSeparator;
    path += gFSUserInfoGmsFile;
    TbDynamicStringTemplate<char> narrowPath;
    UniStringToString(narrowPath, path);
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    gMasterSave->Save(narrowPath.mData);
}
