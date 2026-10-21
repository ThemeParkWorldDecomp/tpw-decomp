# Unit v51110: the server and ride lists' destructors, search members and LoggedInGetList, the FS_OnlineList static initialiser and three
# one-liners (src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp, src/game/ui/ui_main.cpp). The callees outside
# the unit are recording fakes in tests/unit_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystemServerList_ReturnFTPDownloadServe src/game/online/FS_Online.cpp
    src/game/online/FS_OnlineList.cpp src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemServerList_ReturnFTPDownloadServe_KEEP
  __Z17UI_ByeByeBankruptv   # UI_ByeByeBankrupt()
  __Z23sinit_FS_OnlineList_cppv   # sinit_FS_OnlineList_cpp()
  __ZN12ftpdl_callee23ReturnFTPDownloadServerEP25CFSOnlineSystemServerList   # ftpdl_callee::ReturnFTPDownloadServer(CFSOnlineSystemServerList*)
  __ZN13ftpreq_callee11GetFilePathEPK13CFSOnlineRide   # ftpreq_callee::GetFilePath(CFSOnlineRide const*)
  __ZN13ftpreq_callee14ReturnRideInfoEP23CFSOnlineSystemRideListii   # ftpreq_callee::ReturnRideInfo(CFSOnlineSystemRideList*, int, int)
  __ZN13ftpreq_callee18ReturnUploadServerEP25CFSOnlineSystemServerList   # ftpreq_callee::ReturnUploadServer(CFSOnlineSystemServerList*)
  __ZN15CFSOnlineSystem11GetAvatarIDEv   # CFSOnlineSystem::GetAvatarID()
  __ZN18onlinelist2_callee11DestroyRideEP13CFSOnlineRide   # onlinelist2_callee::DestroyRide(CFSOnlineRide*)
  __ZN18onlinelist2_callee13DestroyServerEP15CFSOnlineServer   # onlinelist2_callee::DestroyServer(CFSOnlineServer*)
  __ZN18onlinelist2_callee14RideRemoveHeadEP9TbPtrListI13CFSOnlineRideE   # onlinelist2_callee::RideRemoveHead(TbPtrList<CFSOnlineRide>*)
  __ZN18onlinelist2_callee15DestroyRideListEP9TbPtrListI13CFSOnlineRideE   # onlinelist2_callee::DestroyRideList(TbPtrList<CFSOnlineRide>*)
  __ZN18onlinelist2_callee16DestroyRideInfosEP10ccRideInfo   # onlinelist2_callee::DestroyRideInfos(ccRideInfo*)
  __ZN18onlinelist2_callee16ServerRemoveHeadEP9TbPtrListI15CFSOnlineServerE   # onlinelist2_callee::ServerRemoveHead(TbPtrList<CFSOnlineServer>*)
  __ZN18onlinelist2_callee17DestroyServerListEP9TbPtrListI15CFSOnlineServerE   # onlinelist2_callee::DestroyServerList(TbPtrList<CFSOnlineServer>*)
  __ZN18onlinelist2_callee18DestroyServerInfosEP12ccServerInfo   # onlinelist2_callee::DestroyServerInfos(ccServerInfo*)
  __ZN18onlinelist2_callee9FreeBlockEPv   # onlinelist2_callee::FreeBlock(void*)
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineRideE14ccReceivedListI10ccRideInfoEE15LoggedInGetListEv   # CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccReceivedList<ccRideInfo>>::LoggedInGetList()
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineRideE14ccReceivedListI10ccRideInfoEED1Ev   # CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccReceivedList<ccRideInfo>>::~CFSOnlineSystemList()
  __ZN19CFSOnlineSystemListI9TbPtrListI15CFSOnlineServerE14ccReceivedListI12ccServerInfoEE15LoggedInGetListEv   # CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccReceivedList<ccServerInfo>>::LoggedInGetList()
  __ZN19CFSOnlineSystemListI9TbPtrListI15CFSOnlineServerE14ccReceivedListI12ccServerInfoEED1Ev   # CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccReceivedList<ccServerInfo>>::~CFSOnlineSystemList()
  __ZN23CFSOnlineSystemRideListD1Ev   # CFSOnlineSystemRideList::~CFSOnlineSystemRideList()
  __ZN25CFSOnlineSystemServerListD1Ev   # CFSOnlineSystemServerList::~CFSOnlineSystemServerList()
  _gOnlineListNode1
  _gOnlineListNode2
  _gOnlineListString1
  _gOnlineListString2
)
