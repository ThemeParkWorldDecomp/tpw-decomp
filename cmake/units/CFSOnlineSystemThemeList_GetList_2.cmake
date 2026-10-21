# Unit v5928: the theme list (GetList, RequestListFromServer, StoreReceivedList, ReturnOnlineThemeID), its login callback and two park lists' RequestListFromServer (src/game/online/FS_OnlineList.cpp). The callees are recording fakes in tests/unit_CFSOnlineSystemThemeList_GetList_2.cpp.
set(TPW_UNIT_CFSOnlineSystemThemeList_GetList_2 src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystemThemeList_GetList_2_KEEP
  __Z28ThemeListPostLoginCallbackFniPKv   # ThemeListPostLoginCallbackFn(int, void const*)
  __ZN24CFSOnlineSystemThemeList17StoreReceivedListEPK14ccReceivedListI11ccThemeInfoE   # CFSOnlineSystemThemeList::StoreReceivedList(ccReceivedList<ccThemeInfo> const*)
  __ZN24CFSOnlineSystemThemeList19ReturnOnlineThemeIDEPKc   # CFSOnlineSystemThemeList::ReturnOnlineThemeID(char const*)
  __ZN24CFSOnlineSystemThemeList21RequestListFromServerEP4Cityj   # CFSOnlineSystemThemeList::RequestListFromServer(City*, unsigned int)
  __ZN24CFSOnlineSystemThemeList7GetListEv   # CFSOnlineSystemThemeList::GetList()
  __ZN35CFSOnlineSystemParkList_InCityTop1021RequestListFromServerEP4Cityj   # CFSOnlineSystemParkList_InCityTop10::RequestListFromServer(City*, unsigned int)
  __ZN39CFSOnlineSystemParkList_InWorldByParkID21RequestListFromServerEP4Cityj   # CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer(City*, unsigned int)
  TPW_V5928_STRONG@game/online/FS_OnlineList.cpp   # the #define TPW_V5928_STRONG of src/game/online/FS_OnlineList.cpp (a guard macro; src/game/online/FS_OnlineList.cpp had it)
)
