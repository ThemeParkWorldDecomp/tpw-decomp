# Unit v5762: CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer (src/game/online/FS_OnlineList.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystemParkList_InWorldByPlayerName_Requ.cpp.
set(TPW_UNIT_CFSOnlineSystemParkList_InWorldByPlayerName_Requ src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemParkList_InWorldByPlayerName_Requ_KEEP
  __ZN43CFSOnlineSystemParkList_InWorldByPlayerName21RequestListFromServerEP4Cityj   # CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer(City*, unsigned int)
  TPW_V5762_STRONG@game/online/FS_OnlineList.cpp   # the #define TPW_V5762_STRONG of src/game/online/FS_OnlineList.cpp (a guard macro; src/game/online/FS_OnlineList.cpp had it)
)
