# Unit v5461: the system event callback object (destructor, EventNotification, DeleteAllCallbacks; src/bullfrog/LbSCBack.cpp).
# The callees outside the unit are recording fakes in tests/unit_LbSystemEventCallback_Dtor__LbSystemEventCallbac.cpp.
set(TPW_UNIT_LbSystemEventCallback_Dtor__LbSystemEventCallbac src/bullfrog/LbSCBack.cpp platform/arena.cpp)
set(TPW_UNIT_LbSystemEventCallback_Dtor__LbSystemEventCallbac_KEEP
  __Z19LbSEList_RemoveHeadP9TbPtrListI13_LbSEFunctionE   # LbSEList_RemoveHead(TbPtrList<_LbSEFunction>*)
  __Z19LbSE_OperatorDeletePv   # LbSE_OperatorDelete(void*)
  __Z30LbSystemEventCallback_DtorBodyP22_LbSystemEventCallbacks   # LbSystemEventCallback_DtorBody(_LbSystemEventCallback*, short)
  __ZN22_LbSystemEventCallback17EventNotificationEiPv   # _LbSystemEventCallback::EventNotification(int, void*)
  __ZN22_LbSystemEventCallback18DeleteAllCallbacksEv   # _LbSystemEventCallback::DeleteAllCallbacks()
  _kLbSEListDtorVector
  __ZN7TbTListIP13_LbSEFunctionS1_E10RemoveHeadEv@bullfrog/LbSCBack.cpp   # TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveHead() (an explicit specialization of src/bullfrog/LbSCBack.cpp, declared above its callers since the regroup)
)
