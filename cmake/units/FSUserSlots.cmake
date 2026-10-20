# Unit w48: creating and deleting a player on disk, the global master save's write-back, SetEasyModeUser and the lobby's SendQuit.
# The library is test_FSSystem's (the real string helpers, FS_DelTree, the player slots' paths) with the unit's four files; the
# unit file includes unit_FSSystem.cpp for its fakes.
set(TPW_UNIT_FSUserSlots src/FS_Registry.cpp src/FS_System.cpp src/TPString.cpp src/FS_UserInfo.cpp src/FS_UserInfo_ctor.cpp
    src/FS_UserInfo_slots.cpp src/FS_MasterSave_easy.cpp src/lobby_sendquit.cpp)
