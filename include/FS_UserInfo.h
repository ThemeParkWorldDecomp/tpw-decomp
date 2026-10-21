// FS_UserInfo.cpp (main app 0x1017de54..0x1017fdc8; src/game/save/FS_UserInfo.cpp): the four player
// login slots and the paths of the players' folders. PARTIAL: the start-up path is
// translated, and (units w43, w48) GameLogin, SaveGlobalMasterSave and the creating and deleting of
// users (src/game/save/FS_MasterSave.cpp, src/game/save/FS_UserInfo.cpp); only GameLogout is not yet.
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"
#include "email.h"

enum EFSUserInfoGameUser : int32_t {};

// A login slot (0x18 bytes on the Mac); its constructor and destructor are implicit
// (out-of-line copies 0x1017df80, 0x1017dee8). The constructor leaves mEasyMode alone.
struct TFSUserInfoGameLoginUser {
    TbDynamicStringTemplate<char16_t> mName;   // 0x00  empty for a free slot
    int32_t mEasyMode;                         // 0x14  the user's master save is in easy mode
};

class CFSUserInfo {
public:
    CFSUserInfo();           // 0x1017de54
    ~CFSUserInfo();          // 0x1017dfec (nonvirtual)
    // 0x1017e0a8: reads the slots from the users folder (one folder "<slot><name>" per
    // user, slots 1 to 4) and creates the save, users and online folders that are missing.
    void GameLoginInit();
    // 0x1017e634: creates the user's theme folders that are missing.
    void CheckAllThemeDirectoriesExist(const TbDynamicStringTemplate<char16_t>& path);
    int32_t GetNumValidGameLoginSlots();   // 0x1017e788: the slots with a user
    TbDynamicStringTemplate<char16_t> ReturnPathToSaveDirectory();    // 0x1017e838: <local path>save
    TbDynamicStringTemplate<char16_t> ReturnPathToUsersDirectory();   // 0x1017e8fc: <save>:users
    // The slot's user name, NULL when the slot is empty (0x1017e9d8).
    const TbStringBase<char16_t>* GetGameLoginSlotUserName(EFSUserInfoGameUser slot);
    int32_t IsGameLoginSlotUserEasyMode(EFSUserInfoGameUser slot);    // 0x1017ea74
    // 0x1017ead8: the slot's folder name: its number (from 1) and the user's name. The slot
    // must have a user (the original appends through a NULL reference otherwise).
    TbDynamicStringTemplate<char16_t> ReturnGameLoginSlotUsersDirectoryName(EFSUserInfoGameUser slot);
    // 0x1017ebf0: <users>:<the slot's folder name>.
    TbDynamicStringTemplate<char16_t> ReturnGameLoginSlotUsersPath(EFSUserInfoGameUser slot);
    TbDynamicStringTemplate<char16_t> ReturnUsersPath();            // 0x1017f7f4: the logged-in slot's
    TbDynamicStringTemplate<char16_t> ReturnUsersPathWithTheme();   // 0x1017f858: <users path>:<current theme>
    TbDynamicStringTemplate<char16_t> ReturnOnlinePath();           // 0x1017f980: <save>:online
    // unit w48 (src/game/save/FS_UserInfo.cpp), 0x1017ee38: a slot that holds a player loses the player's folder; then the slot
    // takes the name and the easy mode flag, the player's folders are made, the global master save is replaced by a blank one
    // (easy mode user when the flag is not 0) and written to the player's folder.
    void CreateNewGameLoginUserInSlot(EFSUserInfoGameUser slot, const TbDynamicStringTemplate<char16_t>& name,
                                      int32_t easyMode);
    // unit w48, 0x1017f0b8: the folder `path`, a folder of every theme under it, and (easy mode) the park's easymode file
    // copied from the data folder into each theme's folder where there is one.
    void CreateUsersDirectories(const TbDynamicStringTemplate<char16_t>& path, int32_t easyMode);
    void GameLogin(EFSUserInfoGameUser slot);  // 0x1017f330, src/game/save/FS_UserInfo.cpp (unit w43)
    void GameLogout();                         // 0x1017f61c, untranslated
    // unit w47, translated by unit w48 (src/game/save/FS_UserInfo.cpp), 0x1017ed10: removes the slot's folder, its name and easy flag.
    void DeleteGameLoginUserInSlot(EFSUserInfoGameUser slot);

    // unit w43
    void SaveGlobalMasterSave();   // 0x1017fa50
    // unit w48 (src/game/save/FS_UserInfo.cpp), 0x1017fab0: writes the global master save (made when there is none) to the slot's
    // folder, file gms.dat.
    void InternalSaveGlobalMasterSave(EFSUserInfoGameUser slot);

    TFSUserInfoGameLoginUser mUsers[4];   // 0x00
    int32_t mGameUser;                    // 0x60  the logged-in slot, -1 for none (the lobby then shows the login panel)
    Outbox mOutbox;                       // 0x64
    AddressBook mAddressBook;             // 0xc4
};
extern CFSUserInfo* gUserInfo;   // 0x10380b34, original name unknown
// The strings __sinit_FS_UserInfo_cpp constructs (the same as GameSys.cpp's
// gGameSysUpload and gGameSysGmsFile).
extern TbDynamicStringTemplate<char16_t> gFSUserInfoUpload;    // 0x10380b58 "upload"
extern TbDynamicStringTemplate<char16_t> gFSUserInfoGmsFile;   // 0x10380b38 "gms.dat", a user's master save
