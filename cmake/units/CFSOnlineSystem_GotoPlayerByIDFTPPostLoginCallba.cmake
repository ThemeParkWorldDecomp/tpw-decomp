# Unit v51123: the FTP post-login callbacks, login getters, Login/Logout, three CompleteGetList instances, two std::__tree helpers and two
# status panel members (src/game/online/FS_Online.cpp, src/game/save/FS_MasterSave.cpp, src/game/ui/ui_FeatureStatus.cpp, src/game/ui/ui_bogStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.cpp, which also defines the globals of other files.
set(TPW_UNIT_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba src/game/online/FS_Online.cpp
    src/game/save/FS_MasterSave.cpp src/game/ui/ui_FeatureStatus.cpp src/game/ui/ui_bogStatus.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba_KEEP
  __Z14ParkRemoveHeadP9TbPtrListI13CFSOnlineParkE   # ParkRemoveHead(TbPtrList<CFSOnlinePark>*)
  __Z15ThemeRemoveHeadP9TbPtrListI14CFSOnlineThemeE   # ThemeRemoveHead(TbPtrList<CFSOnlineTheme>*)
  __ZN14BogStatusPanel6RemoveEv   # BogStatusPanel::Remove()
  __ZN15CFSOnlineSystem12GetChildLockEv   # CFSOnlineSystem::GetChildLock()
  __ZN15CFSOnlineSystem14CanWeSendEmailEv   # CFSOnlineSystem::CanWeSendEmail()
  __ZN15CFSOnlineSystem16GetLoginPasswordEv   # CFSOnlineSystem::GetLoginPassword()
  __ZN15CFSOnlineSystem16GetLoginPlayerIDEv   # CFSOnlineSystem::GetLoginPlayerID()
  __ZN15CFSOnlineSystem17LoginRASCancelledEv   # CFSOnlineSystem::LoginRASCancelled()
  __ZN15CFSOnlineSystem18GetLoginCustomerIDEv   # CFSOnlineSystem::GetLoginCustomerID()
  __ZN15CFSOnlineSystem20GetLoginCustomerNameEv   # CFSOnlineSystem::GetLoginCustomerName()
  __ZN15CFSOnlineSystem20GetLoginEmailAddressEv   # CFSOnlineSystem::GetLoginEmailAddress()
  __ZN15CFSOnlineSystem27RequestFTPPostLoginCallbackEiPKv   # CFSOnlineSystem::RequestFTPPostLoginCallback(int, void const*)
  __ZN15CFSOnlineSystem34GotoPlayerByIDFTPPostLoginCallbackEiPKv   # CFSOnlineSystem::GotoPlayerByIDFTPPostLoginCallback(int, void const*)
  __ZN15CFSOnlineSystem40LoggedInGotoPlayerRequestFTPDownloadByIDEv   # CFSOnlineSystem::LoggedInGotoPlayerRequestFTPDownloadByID()
  __ZN15CFSOnlineSystem5LoginEPFviPKvES1_   # CFSOnlineSystem::Login(void (*)(int, void const*), void const*)
  __ZN15CFSOnlineSystem6LogoutEv   # CFSOnlineSystem::Logout()
  __ZN18FeatureStatusPanel14InitViewWindowEv   # FeatureStatusPanel::InitViewWindow()
  __ZN19CFSOnlineSystemListI9TbPtrListI13CFSOnlineParkE14ccReceivedListI10ccParkInfoEE15CompleteGetListEi   # CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccReceivedList<ccParkInfo>>::CompleteGetList(int)
  __ZN19CFSOnlineSystemListI9TbPtrListI14CFSOnlineThemeE14ccReceivedListI11ccThemeInfoEE15CompleteGetListEi   # CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccReceivedList<ccThemeInfo>>::CompleteGetList(int)
  __ZN19CFSOnlineSystemListI9TbPtrListI15CFSOnlineServerE14ccReceivedListI12ccServerInfoEE15CompleteGetListEi   # CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccReceivedList<ccServerInfo>>::CompleteGetList(int)
  __ZN6macstd10TreeUShortC1ERKNS_10LessUShortERKNS_11AllocUShortE   # macstd::TreeUShort::TreeUShort(macstd::LessUShort const&, macstd::AllocUShort const&)
  __ZN6macstd15AllocUShortNode10deallocateEPvm   # macstd::AllocUShortNode::deallocate(void*, unsigned long)
  __ZN6macstd15gTreeUShortWordE   # macstd::gTreeUShortWord
  _gParkRecordDeleter
  _gServerRecordDeleter
  _gThemeRecordDeleter
  __ZN7TbTListIP13CFSOnlineParkS1_E10RemoveHeadEv@game/online/FS_Online.cpp   # TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() (an explicit specialization of src/game/online/FS_Online.cpp, declared above its callers since the regroup)
)
