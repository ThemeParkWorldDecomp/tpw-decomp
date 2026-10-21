# Unit v5016: COnlineControl's constructor, destructor, InitClient and InitGameWorld (src/game/online/online.cpp). The callees
# outside the unit are recording fakes in tests/unit_COnlineControl_COnlineControl.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_COnlineControl src/game/online/online.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_COnlineControl_COnlineControl_KEEP
  __ZN13online_callee15HandleToPointerEj   # online_callee::HandleToPointer(unsigned int)
  __ZN13online_callee15PointerToHandleEPKv   # online_callee::PointerToHandle(void const*)
  __ZN14COnlineControl10InitClientEv   # COnlineControl::InitClient()
  __ZN14COnlineControl13InitGameWorldEv   # COnlineControl::InitGameWorld()
  __ZN14COnlineControlC1Ev   # COnlineControl::COnlineControl()
  __ZN14COnlineControlD1Ev   # COnlineControl::~COnlineControl()
)
