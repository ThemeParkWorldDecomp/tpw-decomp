// FS_UserInfo.cpp (main app), unit w48: creating and deleting a player on disk, the user folders
// and the write-back of the global master save.
// CreateNewGameLoginUserInSlot 0x1017ee38 (524 bytes), DeleteGameLoginUserInSlot 0x1017ed10 (212),
// CreateUsersDirectories 0x1017f0b8 (544), InternalSaveGlobalMasterSave 0x1017fab0 (256).
#include "FS_UserInfo.h"
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPString.h"

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

void CFSUserInfo::InternalSaveGlobalMasterSave(EFSUserInfoGameUser slot) {
    TbDynamicStringTemplate<char16_t> path(SlotPath(this, slot));
    path += gTPGlobals.mWideSeparator;
    path += gFSUserInfoGmsFile;
    TbDynamicStringTemplate<char> narrowPath;
    UniStringToString(narrowPath, path);
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    gMasterSave->Save(narrowPath.mData);
}
