# The lobby's small leaves, part 3 (unit w59): the ride database's one-argument GetTypeName and the two sign-name members, the theme
# gate names, FLY_Close and RSSE_Close (one test module, test_LobbyLeaves3). The strings are the real bullfrog strings; the string
# table, the macdoze compare, the heap, FLY_RemoveBase, KillScript and GAME_ASSERTMSG are fakes in the unit file.
set(TPW_UNIT_LobbyLeaves3 src/RideDB_balance.cpp src/RideDB_names.cpp src/Game_theme.cpp src/TPString.cpp src/hlsprite_flies.cpp src/rsse_close.cpp
    src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
