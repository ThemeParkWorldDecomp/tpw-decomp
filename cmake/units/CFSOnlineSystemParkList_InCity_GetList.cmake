# Unit v5244: CFSOnlineSystemParkList_InCity::GetList (src/game/online/FS_OnlineList.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystemParkList_InCity_GetList.cpp, which also defines gOnlineSystem.
set(TPW_UNIT_CFSOnlineSystemParkList_InCity_GetList src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemParkList_InCity_GetList_KEEP
  __ZN30CFSOnlineSystemParkList_InCity7GetListEiPFviPKvP9TbPtrListI13CFSOnlineParkEES1_   # CFSOnlineSystemParkList_InCity::GetList(int, void (*)(int, void const*, TbPtrList<CFSOnlinePark>*), void const*)
)
