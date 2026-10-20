// FS_UserInfo.cpp and FS_MasterSave.cpp (main app), unit w43.
// GameLogin 0x1017f330 (680 bytes), SaveGlobalMasterSave 0x1017fa50 (40), GiveExtraKey 0x1016a4b4 (16), IsEasyModeUser 0x1016a4fc (8).
#include "FS_UserInfo.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Game.h"
#include "TPGlobals.h"
#include "TPString.h"

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

void CFSGlobalMasterSave::GiveExtraKey() {
    mExtraKeys = static_cast<int32_t>(static_cast<uint32_t>(mExtraKeys) + 1u);
}

bool CFSGlobalMasterSave::IsEasyModeUser() const { return mEasyMode != 0; }

void CFSUserInfo::SaveGlobalMasterSave() {
    InternalSaveGlobalMasterSave(static_cast<EFSUserInfoGameUser>(mGameUser));
}

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
