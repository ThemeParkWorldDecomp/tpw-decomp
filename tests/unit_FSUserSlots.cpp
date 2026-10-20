// The fakes and entry points for unit w48 (src/FS_UserInfo_slots.cpp, src/FS_MasterSave_easy.cpp, src/lobby_sendquit.cpp;
// libtpw_unit_FSUserSlots.dylib, used by test_FSUserSlots.py). The library is test_FSSystem's: unit_FSSystem.cpp is included
// whole for the functional bullfrog strings, the faked file system, the master save's recording constructor and destructor,
// and the real FS_DelTree, UniStringToString and ReturnGameLoginSlotUsersPath behind them. This file adds the callees the
// unit's own functions reach that unit_FSSystem.cpp does not fake: the master save's Save, LbFile_Exists and LbFile_Copy
// (recording), and the lobby global.
#include "unit_FSSystem.cpp"

#include "lobby.h"

// ---- recording fakes of the unit's callees

int32_t CFSGlobalMasterSave::Save(const char* path) {
    ev("CFSGlobalMasterSave::Save", {lab(this)}, {path});
    return static_cast<int32_t>(ret("CFSGlobalMasterSave::Save"));
}
// LbFile_Exists answers from the queue "LbFile_Exists" (0 or 1).
uint8_t LbFile_Exists(const char* name) {
    ev("LbFile_Exists", {}, {name});
    return ret("LbFile_Exists") != 0 ? 1 : 0;
}
TbError LbFile_Copy(const char* from, const char* to) {
    ev("LbFile_Copy", {}, {from, to});
    return TbError(0);
}

CLBLobby* gLobby;

// ---- test entry points

extern "C" {

void* tpw_slots_addr(const char* name) {
    if (std::strcmp(name, "mLevelsFolder") == 0) return &gTPGlobals.mLevelsFolder;
    if (std::strcmp(name, "mExtension") == 0) return &gTPGlobals.mF4434;
    return nullptr;
}

// The master save's easy mode byte (-1 without a master save).
int32_t tpw_slots_easy_byte(const void* save) {
    return save == nullptr ? -1 : static_cast<const CFSGlobalMasterSave*>(save)->mEasyMode;
}
// Sets the byte before a call (the recording constructor leaves it as it finds it).
void tpw_slots_set_easy_byte(void* save, int32_t value) {
    static_cast<CFSGlobalMasterSave*>(save)->mEasyMode = static_cast<uint8_t>(value);
}

// The lobby global: a block of the lobby's size with its state word set (the constructor is not linked).
void* tpw_slots_new_lobby(int32_t state) {
    auto* lobby = static_cast<CLBLobby*>(std::calloc(1, sizeof(CLBLobby)));
    lobby->mState = state;
    gLobby = lobby;
    return lobby;
}
void tpw_slots_clear_lobby() { gLobby = nullptr; }
int32_t tpw_slots_lobby_state() { return gLobby == nullptr ? -1 : gLobby->mState; }
void tpw_slots_set_restart(int32_t value) { gGame.mRestart = value; }
int32_t tpw_slots_restart() { return gGame.mRestart; }

int32_t tpw_slots_call(const char* name, const int64_t* a) {
    const std::string n(name);
    auto* info = reinterpret_cast<CFSUserInfo*>(a[0]);
    const auto slot = static_cast<EFSUserInfoGameUser>(a[1]);
    if (n == "CFSUserInfo::CreateNewGameLoginUserInSlot")
        info->CreateNewGameLoginUserInSlot(slot, *reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(a[2]),
                                           static_cast<int32_t>(a[3]));
    else if (n == "CFSUserInfo::DeleteGameLoginUserInSlot") info->DeleteGameLoginUserInSlot(slot);
    else if (n == "CFSUserInfo::CreateUsersDirectories")
        info->CreateUsersDirectories(*reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(a[1]), static_cast<int32_t>(a[2]));
    else if (n == "CFSUserInfo::InternalSaveGlobalMasterSave") info->InternalSaveGlobalMasterSave(slot);
    else if (n == "CFSGlobalMasterSave::SetEasyModeUser")
        reinterpret_cast<CFSGlobalMasterSave*>(a[0])->SetEasyModeUser(static_cast<int32_t>(a[1]));
    else if (n == "CLBLobby::SendQuit") reinterpret_cast<CLBLobby*>(a[0])->SendQuit();
    else return -12345;
    return 0;
}

}  // extern "C"
