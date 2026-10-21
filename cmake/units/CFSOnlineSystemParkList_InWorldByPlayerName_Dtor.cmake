# Unit v5063: the park list "in the world by player name" (its destructor and GetList; src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp).
# The callees outside the unit, and the list bases' destructors, are recording fakes in
# tests/unit_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor src/game/online/FS_Online.cpp
    src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_KEEP
  __ZN43CFSOnlineSystemParkList_InWorldByPlayerName7GetListEPKDsPFviPKvP9TbPtrListI13CFSOnlineParkEES3_   # CFSOnlineSystemParkList_InWorldByPlayerName::GetList(char16_t const*, void (*)(int, void const*, TbPtrList<CFSOnlinePark>*), void const*)
  __ZN43CFSOnlineSystemParkList_InWorldByPlayerNameD1Ev   # CFSOnlineSystemParkList_InWorldByPlayerName::~CFSOnlineSystemParkList_InWorldByPlayerName()
)
