# Unit v51129: LCString::assign, the horizontal gradient's destructor, the ride list's GetList, the server and theme lists' destructors and the
# theme list's LoggedInGetList (src/game/MslSupport.cpp, src/game/online/FS_OnlineList.cpp, src/game/ui/InterfaceDrawEngine.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_string_assign_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_string_assign_2 src/game/MslSupport.cpp src/game/online/FS_OnlineList.cpp
    src/game/ui/InterfaceDrawEngine.cpp platform/arena.cpp tests/standins/std_basic_string_assign_2.cpp
    tests/standins/std_basic_string_assign_2_callees.cpp)
set(TPW_UNIT_std_basic_string_assign_2_KEEP
  __ZN18onlinelist3_callee12DestroyThemeEP14CFSOnlineTheme   # onlinelist3_callee::DestroyTheme(CFSOnlineTheme*)
  __ZN18onlinelist3_callee15ThemeRemoveHeadEP9TbPtrListI14CFSOnlineThemeE   # onlinelist3_callee::ThemeRemoveHead(TbPtrList<CFSOnlineTheme>*)
  __ZN18onlinelist3_callee16DestroyThemeListEP9TbPtrListI14CFSOnlineThemeE   # onlinelist3_callee::DestroyThemeList(TbPtrList<CFSOnlineTheme>*)
  __ZN18onlinelist3_callee17DestroyThemeInfosEP11ccThemeInfo   # onlinelist3_callee::DestroyThemeInfos(ccThemeInfo*)
  __ZN19CFSOnlineSystemListI9TbPtrListI14CFSOnlineThemeE14ccReceivedListI11ccThemeInfoEE15LoggedInGetListEv   # CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccReceivedList<ccThemeInfo>>::LoggedInGetList()
  __ZN19CFSOnlineSystemListI9TbPtrListI14CFSOnlineThemeE14ccReceivedListI11ccThemeInfoEED1Ev   # CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccReceivedList<ccThemeInfo>>::~CFSOnlineSystemList()
  __ZN23CFSOnlineSystemRideList7GetListEv   # CFSOnlineSystemRideList::GetList()
  __ZN24CFSOnlineSystemThemeListD1Ev   # CFSOnlineSystemThemeList::~CFSOnlineSystemThemeList()
  __ZN28InterfaceDrawEngineHGradientD1Ev   # InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient()
  __ZN8LCString6assignEPKDsj   # LCString::assign(char16_t const*, unsigned int)
)
