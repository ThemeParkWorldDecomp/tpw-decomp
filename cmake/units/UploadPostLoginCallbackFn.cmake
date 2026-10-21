# Unit v5961: seven small online system members (src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp).
# The callees outside the unit are recording fakes in tests/unit_UploadPostLoginCallbackFn.cpp, which also defines the globals of other files.
set(TPW_UNIT_UploadPostLoginCallbackFn src/game/online/FS_Online.cpp src/game/online/FS_OnlineList.cpp
    platform/arena.cpp)
set(TPW_UNIT_UploadPostLoginCallbackFn_KEEP
  __Z25UploadPostLoginCallbackFniPv   # UploadPostLoginCallbackFn(int, void*)
  __Z38GotoPlayerFTPPostGetParkInfoCallbackFniPKvP9TbPtrListI13CFSOnlineParkE   # GotoPlayerFTPPostGetParkInfoCallbackFn(int, void const*, TbPtrList<CFSOnlinePark>*)
  __ZN10ccRideInfo8DestructEPS_s   # ccRideInfo::Destruct(ccRideInfo*, short)
  __ZN10ccRideInfoD1Ev   # ccRideInfo::~ccRideInfo()
  __ZN11ccThemeInfo8DestructEPS_s   # ccThemeInfo::Destruct(ccThemeInfo*, short)
  __ZN11ccThemeInfoD1Ev   # ccThemeInfo::~ccThemeInfo()
  __ZN15CFSOnlineSystem28ReturnConnectionToCityServerEv   # CFSOnlineSystem::ReturnConnectionToCityServer()
  __ZN19CFSOnlineFTPControl10CallbackFnEii   # CFSOnlineFTPControl::CallbackFn(int, int)
  __ZN23CFSOnlineSystemCityList7GetListEPFviPKvP9TbPtrListI13CFSOnlineCityEES1_   # CFSOnlineSystemCityList::GetList(void (*)(int, void const*, TbPtrList<CFSOnlineCity>*), void const*)
)
