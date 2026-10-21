# Unit v5154: CFSOnlineSystemParkList::~CFSOnlineSystemParkList (src/game/online/FS_Online.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.cpp.
set(TPW_UNIT_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark src/game/online/FS_Online.cpp)
set(TPW_UNIT_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark_KEEP
  __ZN23CFSOnlineSystemParkListD1Ev   # CFSOnlineSystemParkList::~CFSOnlineSystemParkList()
  __ZN7TbTListIP13CFSOnlineParkS1_E10RemoveHeadEv@game/online/FS_Online.cpp   # TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() (an explicit specialization of src/game/online/FS_Online.cpp, declared above its callers since the regroup)
)
