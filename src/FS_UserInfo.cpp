// FS_UserInfo.cpp (decompiled from the main app of SimTheme Park 2.04b3: the start-up part
// of 0x1017de54..0x1017f9fc and __sinit_FS_UserInfo_cpp at 0x1017fd58). PARTIAL: see
// FS_UserInfo.h for the functions still to do.
//
// The std::list<std::string>::clear instance at 0x1017fc04 (the address book's inline
// destructor) is the host's std::list.
#include "FS_UserInfo.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "FS_MasterSave.h"
#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "bfl_FileSystem.h"
#include "platform/macdoze.h"

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
