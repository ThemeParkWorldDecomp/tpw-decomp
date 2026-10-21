# Unit v5662: CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::CompleteGetList (src/game/online/FS_Online.cpp). The callees outside the unit are recording fakes in
# tests/unit_CFSOnlineSystemList_CompleteGetList.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystemList_CompleteGetList src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemList_CompleteGetList_KEEP
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineRideE14ccReceivedListI10ccRideInfoEE15CompleteGetListEi   # CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccReceivedList<ccRideInfo>>::CompleteGetList(int)
  _gRideRecordDeleter
)
