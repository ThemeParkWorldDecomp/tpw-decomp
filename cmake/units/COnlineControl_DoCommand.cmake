# Unit v5636: COnlineControl::DoCommand 0x10219bec (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_DoCommand.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_DoCommand src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_DoCommand_KEEP
  __ZN14COnlineControl9DoCommandEPv   # COnlineControl::DoCommand(void*)
  _gVoteSendData
)
