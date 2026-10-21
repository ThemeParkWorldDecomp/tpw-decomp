# Unit v5994: the city record's constructor and destructor body, the park record's assignment, the server record's destructor and the ride
# list's post-login callback (src/game/online/FS_OnlineList.cpp). The callees outside the unit
# are recording fakes in tests/unit_CFSOnlineCity_CFSOnlineCity.cpp.
set(TPW_UNIT_CFSOnlineCity_CFSOnlineCity src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineCity_CFSOnlineCity_KEEP
  __Z27RideListPostLoginCallbackFniPKv   # RideListPostLoginCallbackFn(int, void const*)
  __Z28CFSOnlineServer_DestructBodyP15CFSOnlineServer   # CFSOnlineServer_DestructBody(CFSOnlineServer*)
  __ZN13CFSOnlineCity8DestructEs   # CFSOnlineCity::Destruct(short)
  __ZN13CFSOnlineCityC1EPK10ccCityInfoP23CFSOnlineSystemCityList   # CFSOnlineCity::CFSOnlineCity(ccCityInfo const*, CFSOnlineSystemCityList*)
  __ZN13CFSOnlineParkaSERKS_   # CFSOnlinePark::operator=(CFSOnlinePark const&)
  __ZN17onlinelist_callee10ServerFindEP9TbPtrListI15CFSOnlineServerEPS1_   # onlinelist_callee::ServerFind(TbPtrList<CFSOnlineServer>*, CFSOnlineServer*)
  __ZN17onlinelist_callee11CityAddTailEP9TbPtrListI13CFSOnlineCityEPS1_   # onlinelist_callee::CityAddTail(TbPtrList<CFSOnlineCity>*, CFSOnlineCity*)
  __ZN17onlinelist_callee12CityRemoveAtEP9TbPtrListI13CFSOnlineCityERPv   # onlinelist_callee::CityRemoveAt(TbPtrList<CFSOnlineCity>*, void*&)
  __ZN17onlinelist_callee14ServerRemoveAtEP9TbPtrListI15CFSOnlineServerERPv   # onlinelist_callee::ServerRemoveAt(TbPtrList<CFSOnlineServer>*, void*&)
  __ZN17onlinelist_callee8CityFindEP9TbPtrListI13CFSOnlineCityEPS1_   # onlinelist_callee::CityFind(TbPtrList<CFSOnlineCity>*, CFSOnlineCity*)
  __ZN17onlinelist_callee8FreeCityEPv   # onlinelist_callee::FreeCity(void*)
)
