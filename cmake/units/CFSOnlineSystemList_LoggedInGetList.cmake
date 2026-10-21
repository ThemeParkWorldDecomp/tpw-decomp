# Unit v5215: the city list's LoggedInGetList and CompleteGetList (src/game/online/FS_OnlineList.cpp). The callees outside the unit are
# recording fakes in tests/unit_CFSOnlineSystemList_LoggedInGetList.cpp, which also defines gOnlineSystem.
set(TPW_UNIT_CFSOnlineSystemList_LoggedInGetList src/game/online/FS_OnlineList.cpp src/game/online/FS_Online.cpp
    platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemList_LoggedInGetList_KEEP
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineCityE14ccReceivedListI10ccCityInfoEE15CompleteGetListEi   # CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccReceivedList<ccCityInfo>>::CompleteGetList(int)
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineCityE14ccReceivedListI10ccCityInfoEE15LoggedInGetListEv   # CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccReceivedList<ccCityInfo>>::LoggedInGetList()
  _gCityRecordDeleter
  _gOnlineListCallbackCaller
  __ZN7TbTListIP13CFSOnlineCityS1_E10RemoveHeadEv@game/online/FS_Online.cpp   # TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead() (an explicit specialization of src/game/online/FS_Online.cpp, declared above its callers since the regroup)
)
