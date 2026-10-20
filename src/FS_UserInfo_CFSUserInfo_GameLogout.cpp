// CFSUserInfo::GameLogout (main app 0x1017f61c, 424 bytes): the logged-in player's address book, outbox and global master save
// are written to the player's folder, the master save in memory is dropped, the settings are saved to <save>:<:><Config.tcf>
// and the slot is cleared (mGameUser = -1).
#include "FS_UserInfo.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPSettings.h"
#include "TPString.h"
#include "bfl_FileSystem.h"
#include "platform/macdoze.h"

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
