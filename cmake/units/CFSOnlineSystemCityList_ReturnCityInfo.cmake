# Unit v51065: the online lists' small members, two destructors, the park lists' GetList, the login callbacks and the FTP getters
# (src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp, src/game/save/FS_MasterSave.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFSOnlineSystemCityList_ReturnCityInfo.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystemCityList_ReturnCityInfo src/game/online/FS_Online.cpp src/game/online/FS_OnlineList.cpp
    src/game/save/FS_MasterSave.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemCityList_ReturnCityInfo_KEEP
  __Z22AllocatorUShortDestroyPvPt   # AllocatorUShortDestroy(void*, unsigned short*)
  __Z24PostLoginCallbackFn_CityiPKv   # PostLoginCallbackFn_City(int, void const*)
  __Z24PostLoginCallbackFn_ParkiPKv   # PostLoginCallbackFn_Park(int, void const*)
  __ZN15CFSOnlineSystem22GetFTPDownloadParkInfoEv   # CFSOnlineSystem::GetFTPDownloadParkInfo()
  __ZN15CFSOnlineSystem26ReturnFTPDownloadLocalPathEv   # CFSOnlineSystem::ReturnFTPDownloadLocalPath()
  __ZN16online_list_base17ThemeListBaseDtorEP19CFSOnlineSystemListI9TbPtrListI14CFSOnlineThemeE14ccReceivedListI11ccThemeInfoEE   # online_list_base::ThemeListBaseDtor(CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccReceivedList<ccThemeInfo>>*)
  __ZN16park_info_callee14ReturnCityInfoEP23CFSOnlineSystemCityListi   # park_info_callee::ReturnCityInfo(CFSOnlineSystemCityList*, int)
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineParkE14ccReceivedListI10ccParkInfoEE15LoggedInGetListEv   # CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccReceivedList<ccParkInfo>>::LoggedInGetList()
  __ZN23CFSOnlineSystemCityListD1Ev   # CFSOnlineSystemCityList::~CFSOnlineSystemCityList()
  __ZN24CFSOnlineSystemThemeList21ReturnOnlineThemeNameEi   # CFSOnlineSystemThemeList::ReturnOnlineThemeName(int)
  __ZN30CFSOnlineSystemParkList_InCity14ReturnParkInfoEi   # CFSOnlineSystemParkList_InCity::ReturnParkInfo(int)
  __ZN35CFSOnlineSystemParkList_InCityTop107GetListEiPFviPKvP9TbPtrListI13CFSOnlineParkEES1_   # CFSOnlineSystemParkList_InCityTop10::GetList(int, void (*)(int, void const*, TbPtrList<CFSOnlinePark>*), void const*)
  __ZN39CFSOnlineSystemParkList_InWorldByParkID7GetListEiPFviPKvP9TbPtrListI13CFSOnlineParkEES1_   # CFSOnlineSystemParkList_InWorldByParkID::GetList(int, void (*)(int, void const*, TbPtrList<CFSOnlinePark>*), void const*)
)

set(TPW_UNIT_CFSOnlineSystemCityList_ReturnCityInfo_OWN_CALLLOG ON)
