# Unit v5921: three online system destructors (src/game/online/FS_Online.cpp). The callees outside the unit are
# recording fakes in tests/unit_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.cpp.
set(TPW_UNIT_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2 src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2_KEEP
  __ZN10ccCityInfoD1Ev   # ccCityInfo::~ccCityInfo()
  __ZN15CFSOnlineSystemD1Ev   # CFSOnlineSystem::~CFSOnlineSystem()
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineParkE14ccReceivedListI10ccParkInfoEED1Ev   # CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccReceivedList<ccParkInfo>>::~CFSOnlineSystemList()
  __ZN7TbTListIP13CFSOnlineParkS1_E10RemoveHeadEv@game/online/FS_Online.cpp   # TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() (an explicit specialization of src/game/online/FS_Online.cpp, declared above its callers since the regroup)
)
