# Unit v5063: the park list "in the world by player name" (its destructor and GetList; src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.cpp).
# The callees outside the unit, and the list bases' destructors, are recording fakes in
# tests/unit_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.cpp platform/arena.cpp)
