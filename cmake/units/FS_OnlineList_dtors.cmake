# Unit v5L17: the online list destructors of the online system's teardown (src/game/online/FS_Online.cpp). The park list's destructor and the
# park instance of the base (and with it the rest of unit v5921's file), and unit v51065's file for the city list's destructor, are
# compiled too: the test runs the host's real destructor chains. The callees are fakes in tests/unit_FS_OnlineList_dtors.cpp.
set(TPW_UNIT_FS_OnlineList_dtors src/game/online/FS_Online.cpp src/game/online/FS_OnlineList.cpp
    src/game/save/FS_MasterSave.cpp platform/arena.cpp)
set(TPW_UNIT_FS_OnlineList_dtors_KEEP
  __Z22AllocatorUShortDestroyPvPt   # AllocatorUShortDestroy(void*, unsigned short*)
  __Z24PostLoginCallbackFn_CityiPKv   # PostLoginCallbackFn_City(int, void const*)
  __Z24PostLoginCallbackFn_ParkiPKv   # PostLoginCallbackFn_Park(int, void const*)
  __ZN10ccCityInfoD1Ev   # ccCityInfo::~ccCityInfo()
  __ZN15CFSOnlineSystem22GetFTPDownloadParkInfoEv   # CFSOnlineSystem::GetFTPDownloadParkInfo()
  __ZN15CFSOnlineSystem26ReturnFTPDownloadLocalPathEv   # CFSOnlineSystem::ReturnFTPDownloadLocalPath()
  __ZN15CFSOnlineSystemD1Ev   # CFSOnlineSystem::~CFSOnlineSystem()
  __ZN16online_list_base17ThemeListBaseDtorEP19CFSOnlineSystemListI9TbPtrListI14CFSOnlineThemeE14ccReceivedListI11ccThemeInfoEE   # online_list_base::ThemeListBaseDtor(CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccReceivedList<ccThemeInfo>>*)
  __ZN16park_info_callee14ReturnCityInfoEP23CFSOnlineSystemCityListi   # park_info_callee::ReturnCityInfo(CFSOnlineSystemCityList*, int)
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineCityE14ccReceivedListI10ccCityInfoEED1Ev   # CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccReceivedList<ccCityInfo>>::~CFSOnlineSystemList()
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineParkE14ccReceivedListI10ccParkInfoEE15LoggedInGetListEv   # CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccReceivedList<ccParkInfo>>::LoggedInGetList()
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineParkE14ccReceivedListI10ccParkInfoEED1Ev   # CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccReceivedList<ccParkInfo>>::~CFSOnlineSystemList()
  __ZN23CFSOnlineSystemCityListD1Ev   # CFSOnlineSystemCityList::~CFSOnlineSystemCityList()
  __ZN23CFSOnlineSystemParkListD1Ev   # CFSOnlineSystemParkList::~CFSOnlineSystemParkList()
  __ZN24CFSOnlineSystemThemeList21ReturnOnlineThemeNameEi   # CFSOnlineSystemThemeList::ReturnOnlineThemeName(int)
  __ZN30CFSOnlineSystemParkList_InCity14ReturnParkInfoEi   # CFSOnlineSystemParkList_InCity::ReturnParkInfo(int)
  __ZN35CFSOnlineSystemParkList_InCityTop107GetListEiPFviPKvP9TbPtrListI13CFSOnlineParkEES1_   # CFSOnlineSystemParkList_InCityTop10::GetList(int, void (*)(int, void const*, TbPtrList<CFSOnlinePark>*), void const*)
  __ZN35CFSOnlineSystemParkList_InCityTop10D1Ev   # CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10()
  __ZN39CFSOnlineSystemParkList_InWorldByParkID7GetListEiPFviPKvP9TbPtrListI13CFSOnlineParkEES1_   # CFSOnlineSystemParkList_InWorldByParkID::GetList(int, void (*)(int, void const*, TbPtrList<CFSOnlinePark>*), void const*)
  __ZN39CFSOnlineSystemParkList_InWorldByParkIDD1Ev   # CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID()
  __ZN7TbTListIP13CFSOnlineParkS1_E10RemoveHeadEv@game/online/FS_Online.cpp   # TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() (an explicit specialization of src/game/online/FS_Online.cpp, declared above its callers since the regroup)
)
